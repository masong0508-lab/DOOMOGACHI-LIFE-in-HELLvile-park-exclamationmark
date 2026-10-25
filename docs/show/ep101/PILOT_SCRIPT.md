# DOOMOGACHI LIFE in HELLvile Park!

## Episode 101: "Mingle Night"

### The complete pilot script

| | |
|---|---|
| **Series** | DOOMOGACHI LIFE in HELLvile Park!, an adult animated comedy-horror adapted from the game *DOOMOGACHI LIFE* |
| **Episode** | 101, "Mingle Night" (the pilot) |
| **Rating** | TV-MA (V), 18+. DOOM-level gore through the cute/brutal filter, only where the beat sheet puts it, always on demons. No sexual or suggestive material, no swearing ("Fudge!" is the strongest word), and no child in any frame with gore |
| **Running time** | 22:00 exactly (00:00:00:00 to 00:22:00:00 at 30 fps): cold open 1:40, main title 0:50, four acts, three eyecatch pairs, tag, end credits and a post-credits button |
| **Length** | About 60 pages (3,313 screenplay lines at 55 lines a page; title page, cast list and production timing notes not counted) |
| **Draft** | Production draft for the table read and scratch recording, 25 October 2026. Assembled from the four supervised script parts (writers 1-4, the punch-up pass and the script supervisor's pass) |
| **Written by** | [writing credit TBD] · Created by [creator credit TBD] |
| **Sources** | Timing: `docs/show/PILOT_BEATSHEET.md` (wins every conflict). Characters and voices: `PILOT_TREATMENT.md`, `docs/design/story/story_final.md`. Comic timing: `GAG_BIBLE.md`. Decisions D1-D18 at their recommended defaults (`PILOT_ROADMAP.md`). Main title lyric: "Doomtastic Times", `LYRICS.md` (final lyrics) |
| **Line IDs** | Every spoken line ends with its line ID in an HTML comment (`<!-- a1_08_mayor_03 -->`), which the rendered page hides. The same IDs are in `lines.csv`, the actors' sides (`sides/`) and the show engine's `lines.shw`. IDs are provisional until the freeze, before the first take is recorded |

---

## Cast

### Speaking roles (12)

| Role | Who | Lines | First heard |
|---|---|---|---|
| **MAYOR GIGGS** (he, 40s) | Mayor Gigglesworth: a round cherub with busy little wings, a top hat on a pompadour, a handlebar moustache and a bow tie. The town's showman and its chief non-looker | 35 | CO-5 (the hum) |
| **NANA PUDDIN** (she, about 70) | The sheepkin head baker. Calls everyone "sweet pea" and bakes two of everything, out of habit | 22 | 1-09 |
| **PIP** (they, a child; voiced by an adult) | The tiniest bunnykin in town and the whole Junior Festival Patrol, with MR. FLOPS, a toy bunny with one floppy ear | 16 | 1-10 |
| **FATHER FUZZ** (he, 40s) | The plushie bear priest of St. Cuddles. Believes nothing is unhuggable | 16 | 2-05 |
| **BONEJANGLES** (he, 50s) | Hillvile's skeleton bandleader and xylophone teacher, who plays his own ribs. Possessed for most of the night | 15 | 1-08 |
| **CLINKER** (she, 40s-50s; heard, never seen) | The name behind the breath: a Hellvile gate labourer who writes chalk poems and rates everyone else's. Her voice sits 8 frames under the possessed Bonejangles | 12 | 1-14 (inside the fog) |
| **DR. BLOOM** (she, 30s-40s) | A ghoul doctor in pastel scrubs, with cold hands and a dry wit | 5 | 2-05 |
| **STEAMY** (she, 30s) | The frogling bath-house keeper, who knows every rubber duck by name except one | 5 | 1-04 |
| **KEVIN** (he, 30s) | Runs face painting. Three eyes. Nobody ever asks about the third one | 3 | 1-05 |
| **HATTIE HAHA** (she, 30s-40s) | The plushie comedian. Her jokes are about not saying things | 3 | 1-06 |
| **MISS LULLA** (she, 30s) | The sheepkin daycare worker who can nap any baby in under a minute | 1 | 1-07 |
| **GURGLE** (it; heard through the sealed gate) | A belly behind the gate. Its words live in the captions; the same performer gives the purr | 3 + the purr | PC-1 |

### Silent and non-verbal roles

| Role | Note |
|---|---|
| **SUNNY** (they, 20s) | The lead: a bunnykin newcomer with one suitcase and ears that notice everything. **Silent: no lines, no breaths, no efforts.** Every Sunny moment is picture and foley |
| **THE PET** (it) | Hatches in 2-04. Says only "beep." (the raw piezo sound, six times, nobody performs it). Nana's "...He's teething." is the one time it is "he" |
| **MASCOTTY** (she) | A figure in a giant bunny head, perfectly still in every crowd. Silent |
| **GRUMBLEGUTS** (he) | **Not seen in the pilot.** Heard only: a gurgle, padlocks, a BOING, and one low purr after the credits. He debuts in Ep 102 |
| **THE GIGGLING FOUNTAIN** | Two stone faces that giggle 8 frames late (two library sound takes) |
| **WALLA** | The Giggle crowd, Hattie's tent crowd, the storybook kids, a laugh beyond the Wall and the fog's many voices: 22 scripted walla lines, best left to TTS |
| Also seen | MR. FLOPS (Pip's toy), RIVET (a welding mask and a signature, "—R"), GRAMPY (main title), Puff Demons and the Big Puff |

### How to read the dialogue

* Gaps are frames at 30 fps: `[R5]` 0.17 s · `[R8]` 0.27 s · `[R13]` 0.43 s · `[R21]` 0.70 s · `[R26]` 0.87 s. You don't have to count them. Each piece of speech on either side of a gap is its own recording, and the engine places the gap.
* `{d}` = deadpan (flat, no smile, no music under you). `{w}` = wavy (the fog). `{big}...{/}` = the word lands whole. `{..}` = a 0.27 s pause inside the line. A word in CAPITALS is stressed, not shouted (Pip's "CLINKER!" is the one shout). `...` at the start of a line means come in a little late and small. A `/` inside a line is a line break in the dialogue box, not a pause.
* `POSSESSED BONEJANGLES / CLINKER` lines are two recordings with the same words; Clinker is recorded in sync, and the engine slides her 8 frames late.
* Comic-timing marks in the action: `[HOLD 13]` a held frame count, `[FREEZE 13]` everything stops, `[MUTE]` the score cuts, `[BUTTON +13]` the topper 13 frames after the joke, `[INSERT 4]` / `[TRUTH 4]` a 4-frame whiplash insert, `[ONE CHANGE]` the frame comes back with exactly one thing different, `[NO BUTTON]` a beat that must not get a gag, `[!RIPPLE]` a crowd reaction staggered 0-3 frames, `3x@15` three hits 15 frames apart, `FC` a frame check for the animators.
* `**Timing.**` paragraphs after each scene are production notes: the beat length, the word count and how the dialogue fits. Each scene's timecode line reads `start-end · length · word cap · words used`.

---

## COLD OPEN, "The Last Mingle"
`00:00-01:40 · 1:40 · 6 beats · shots 1-21 · 4 words`

> **Refresh note.** This is `PILOT_COLD_OPEN.md` reproduced shot for shot in the shared format, with the shot list's timings. The TV-MA re-aim of the beat sheet changed one thing, and SC05 marks it **[TV-MA refresh]**: the third bleed's red is *paint* (CANDY's deepest red, not blood), boarded as an A/B with a pink drip (`PILOT_ROADMAP.md` D16). Nothing else changes: no new lines and no re-timing. The same note is now in `PILOT_COLD_OPEN.md` too.

### SC01 · CO-1 · INSERT - A CALENDAR PAGE - CRAYON STORYBOOK
`00:00-00:03 · 0:03 · shot 1 · cap 0 · used 0`

FADE IN:

Everything is drawn in children's-book crayon on paper grain. The lines boil gently on twos, the way a child's drawing would if it could breathe.

CRAYON SCRATCH on frame 0, the first sound of the series. A crayon circles one square, MIDSUMMER, then scribbles fireworks all round it, happily, outside the lines.

[+60] MUSIC: a MUSIC BOX starts a sweet little tune in F. This is the MINGLE.

**Timing.** 3.0 s. No dialogue.

### SC02 · CO-2 · EXT. HELLVILE PARK - BANDSTAND - NIGHT (STORYBOOK, LONG AGO)
`00:03-00:33 · 0:30 · shots 2-8 · cap 0 (walla only) · used 0`

SHWIP. A circle iris opens (13 f) on ONE park. No Wall. A river with a bridge. Dead centre of frame, a round bandstand strung with lanterns.

Two long tables: a tower of cupcakes on the north side, a cauldron of soup on the south.

SUPER (in a child's handwriting, typing on): *Every Midsummer night, the whole park had one party.* `s_co02`

Kids run across the bridge both ways, little bunnies one way and little imps the other [!RIPPLE]. Nobody minds. Their babble floats up from far away.

> **KIDS** (WALLA, FAR)  
> *(a soft bed; no word stands out)*  
> Cupcakes! <!-- w_kids_01 -->  
> Wait for me! <!-- w_kids_02 -->  
> Soup! There's soup! <!-- w_kids_03 -->  
> Look, lanterns! <!-- w_kids_04 -->  
> Race you to the bridge! <!-- w_kids_05 -->  
> Hi! Hi! Hi! <!-- w_kids_06 -->

SUPER: *It was called the Mingle.* `s_co03`

From behind: a SMALL CHERUB, all wings and halo, holds up a string. We never see the cherub's face. A small CLOUD bobs on the end of the string, with two dot eyes and a smile. It blinks.

At the soup table, two small horned hands, sooty grey, set down two bowls. Clink. [15 f] Clink.

Crayon fireworks bloom over the bandstand on alternate beats: soft thumps, no bangs, each bloom ramping up over 8 f. The cloud rises to look. Its eyes POP into hearts.

SUPER: *It loved fireworks.* `s_co06`

CLOSE ON the small hand and the loop of string. A burst lights it. [R8] The loop slips off the fingers. A single PLUCK. [HOLD 13] The hand stays open.

SUPER: *The string slipped. Nobody noticed.* `s_co07`

The fireworks fade to embers. The cloud drifts south over the river after the last spark, getting smaller. It does not look back.

[MUTE 29] The music box stops mid-phrase on one particular note. Mark it on the cue sheet: it is the Mayor's note. Then the tune picks up again, softer.

SUPER: *Not everybody went home.* `s_co08`

SMASH CUT TO:

**Timing.** 30.0 s. No cast dialogue. The kids' walla sits at -28 to -20 dB under shots 2-3 only.

### SC03 · CO-3 · EXT. HELLVILE PARK - BANDSTAND - NIGHT (STORYBOOK, LATER)
`00:33-00:41 · 0:08 · shot 9 · cap 0 · used 0`

THUD. [SHK_RUMBLE 13 f] The same bandstand from the same angle, with a WALL straight through the middle of it, exactly on the centre line. The crayon has lost its pink.

South of the Wall: a table with two bowls. North of it: a table with one cupcake.

[SILENCE] No music at all. This is the first silence of the series. Never trim it.

[+2 s] SUPER (appears all at once, no typing): *Nobody talks about why.* `s_co09`

**Timing.** 8.0 s, about 7 s of it silent. No dialogue.

### SC04 · CO-4 · EXT. HELLVILE PARK - BANDSTAND BANNER - NIGHT (STORYBOOK)
`00:41-00:56 · 0:15 · shots 10-11 · cap 0 · used 0`

Over the north half of the bandstand hangs a crayon banner: MINGLE.

MUSIC: the music box comes back, thinner, with one voice.

A pink paint roller slides in from the left, squeaking, and crosses in 30 f. It leaves a new word behind it, still wet: GIGGLE.

SUPERS pop on, one per downbeat: *Now there's a different party.* / *Same night.* / *Half a park.* `s_co11_a..c`

The music stops.

SUPER (appears): *No fireworks.* `s_co11_d`

[R26] A beat of silence. Then the music box takes up its phrase again, alone, and holds one note into the cut.

MATCH CUT TO:

**Timing.** 15.0 s. No dialogue. "Half a park." gets the first laugh, and "No fireworks." is the break.

### SC05 · CO-5 · EXT. HELLVILE PARK - BANDSTAND - DAWN (MIDSUMMER)
`00:56-01:17 · 0:21 · shots 12-18 · cap 4 · used 4 (frame-locked)`

The same banner from the same angle, on the same letter baseline, but real now, in the show's own warm pixel art. GIGGLE, in wet pink paint. A drip.

The held note becomes a HUM at exactly the same pitch. Pull back to find MAYOR GIGGS (40s, he), a round cherub with busy little wings, a top hat on a pompadour, a handlebar moustache and a bow tie, up a ladder with a paint roller. The south half of the bandstand behind him is boarded up, and the Wall stands behind the boards.

> **MAYOR**  
> *(hums the Mingle; stops dead)*  
> Mm-mm, mm-mm-mm, mm-mm-mm, mm-- <!-- co12_hum -->

[+120] He stops on the same note the music box stopped on. [R8] He blinks.

> **MAYOR**  
> *(two falling notes; smile pops second)*  
> ...Hee. Hee. <!-- co13_heehee -->

[R8] Below, the GIGGLING FOUNTAIN's two stone faces giggle back, a quarter-second late. CC: *[the fountain giggles, late]*

Behind the pink paint, the wet N of MINGLE blooms back through.

[3x@15] He rolls over it: squeak. It blooms back, bigger. He rolls again: squeak, lower. Bigger still. He rolls again: squeak, lowest. It blooms back biggest of all, red this time, and a red drip runs down.

**[TV-MA refresh]** *The red is paint, CANDY's deepest red and not blood. The BLOOD ramp stays out of the cold open because children are on screen in it. This is the series' first red. The animatic boards this shot as an A/B, with the drip kept pink in the B version (`PILOT_ROADMAP.md` D16).*

[R13] He whips out a cupcake balloon [POP], ties it over the N (TINK) and pats it twice.

> **MAYOR**  
> *(pleased with himself, quiet)*  
> Perfect. <!-- co15_perfect -->

He glances south, at the boarded-up half of the bandstand and the Wall beyond it.

[FREEZE 13] He stops. His wings stop.

[INSERT 4: crayon. The small empty hand, the loop of string sliding away. PLUCK owns the audio.]

[ONE CHANGE: back on the same frame, the cupcake balloon has turned to face the Wall.]

[+2 f] A small rubber creak. [R8] The Mayor's eyes slide to it. [R13] He reaches up and turns it back to face the town. Squeak.

> **MAYOR**  
> *(a firm pat, as to a pet)*  
> Stay. <!-- co18_stay -->

He climbs down briskly. Clack, clack, clack. His wings start up again on the way down. The balloon stays put.

For now.

**Timing.** 21.0 s. 4 words, about 2.0 s of speech ("...Hee. Hee." 1.07 s, "Perfect." 0.57 s, "Stay." 0.40 s), plus the 4.0 s hum. Every line is frame-locked to the shot list: shot 12 hum +0..+120, shot 13 +4..+36, shot 15 +153..+170, shot 18 +38..+50.

### SC06 · CO-6 · EXT. FOUNTAIN PLAZA / NANA'S BAKERY WINDOW - DAWN
`01:17-01:40 · 0:23 · shots 19-21 · cap 0 · used 0`

A lantern-string *fwip*. High and wide: HaHaland waking up pastel. MUSIC: HaHaland Day in a dawn arrangement, music box and celesta, low. Dawn birds, distant hammering, the fountain.

Citizens string lanterns between the lamp posts, a ripple of bunnies, bears and frogs [!RIPPLE]. A poster on a lamp post: MIDSUMMER GIGGLE TONIGHT! (half the park, all the fun!) `s_co19_poster`

At the far edge of the plaza stands a figure in a giant bunny head: MASCOTTY (she). She is perfectly still, in a crowd that never stops moving.

We drift across the plaza toward the bakery.

NANA'S BAKERY WINDOW. An oven TIK. There are two of everything in the window: two loaves, two buns, two cakes.

A woolly paw (NANA PUDDIN's) tucks a tea towel round an EGG in a basket and sets a small lamp beside it. *Tink.* The paw stays a moment [R21], then goes.

A tag on the basket turns in the warm air: *for whoever's next* `s_co20_tag`

CLOSE ON THE EGG. A basket creak.

Far off, a bus horn plays two notes, the first two of the theme's chorus hook. [30 f] The egg wobbles. TIK.

The horn again, nearer, three notes this time. [30 f] The egg wobbles harder. TIK, lower.

The score ducks out. SHWIP: a heart iris closes on the egg in 5 f and holds there, in silence, for about 2 s.

CRACK.

SMASH CUT TO:

**Timing.** 23.0 s. No dialogue. The CRACK is frame 0 of the main title (00:01:40:00) and the song's first downbeat.

---

## MAIN TITLE, "Doomtastic Times" (main title cut)
`SC07-SC13 · 01:40-02:30 · 0:50 · a locked block · no cast dialogue`

This is a montage, boarded by song section. The vocal belongs to the theme's singer, not to a cast role. The lyric is quoted below from the theme's `LYRICS.md` (final lyrics, main title cut). The beat sheet's section TCs are provisional. The master (50.35 s, 150 bpm, 1 bar = 1.6 s = 48 f) splits as: intro bars 2-9, verse 10-13, the *wall window* bar 14, pre-chorus 15-16, chorus 17-24, post-chorus 25-28, button 29-30. The main title is re-boarded by section at conform, and beat 1-01 absorbs up to ±5 s.

> **Main title lyric (conformed to `LYRICS.md`, final lyrics, main title cut).** Sung by the theme's singer, not by a cast role. The revised lyric no longer sings the theme line anywhere, so the verse-tag placeholder is lifted; the series still spends that line only in dialogue, three times, starting in 1-08. Verse (V1, V3, V5): *"Ohayo, HaHaland! The fountain goes 'Ha!'" / "So all of us smile back," / "Paint it pink, paint it over the crack!"* ("crack!" is clipped to a stop-consonant button into the wordless wall window bar). Pre-chorus (P4): *"Knock, knock, knock! Come out!"*, and the gate answers with KEEP OUT. FOREVER. Chorus (C1-C7): *"Doomtastic Times! (Hey!) / Out of sight and out of mind! / Doomtastic Times! / Out of breath and out of time! / Say it out loud! Out loud! / We're on the safe side! / Doomtastic, Doomtastic Times!"* Button (O1): *"Doomtastic Times!"* C2 still needs the canon owner's sign-off (pre-approved fallback, same notes: *"Out of sight and up all night!"*).

| SC | Beat · TC | Song section | Picture and sound |
|---|---|---|---|
| 07 | MT-1 · 01:40-01:46 | Intro | A white flash off the crack (3 f at most, reduced luminance). The camera cranes up over pastel HaHaland. The logo's letters DROP on the intro hits and squash wide as they land. |
| 08 | MT-2 · 01:46-01:59 | Verse, then the wall window bar | The cast POPs in, one per beat (4-6 f, overshoot), each with a CUTE-skin name plate: Sunny and the suitcase; the pet; Mayor Giggs; Nana; Pip with Mr. Flops; Father Fuzz; Dr. Bloom; Steamy; Hattie; Grampy; Bonejangles at his ribs; Rivet, shown only as a welding mask and sparks. **Pilot variant: Grumbleguts does not pop in.** Mascotty never pops in. She is in every verse shot, perfectly still, and nobody points her out. The verse ends on "...over the crack!" On the wordless wall window bar, the camera arrives at the Wall to the WALL motif alone. |
| 09 | MT-3 · 01:59-02:06 | Pre-chorus | A brick wipe runs down the Wall line and opens a split screen. Both sides run at the Wall, mirrored to the frame, and knock on the same beat (the song's two snare hits). The Hellvile side is silhouettes only. |
| 10 | MT-4 · 02:06-02:08 | The drop, at the end of the pre-chorus | A record scratch is the drop. A tape-stop, then 14 f of near-silence, and the camera stops on the gate: seven padlocks, KEEP OUT. FOREVER., and a small barred grille with its shutter shut. |
| 11 | MT-5 · 02:08-02:21 | Chorus | On the lift, hellify rots the verse shots pixel by pixel (0 to 16 over 21 f). **FC (standards):** the rot runs only on the verse shots with no child in them; Pip's pop-in is not reprised under hellify, so the filter stays on in every frame with a child. A first-person corridor: a PUFF DEMON, all fluff and fang, bobs toward the lens. On the chorus's sweetest line (C6, "We're on the safe side!") the filter drops on "side!", and the demon bursts into red mist and gibs that slap the lens and slide off on twos by the next bar. A tooth *tink*s off the glass. The vocal keeps smiling. The red stays local to the burst, and there is no child in the corridor. Three candles go out in rhythm, on beats 1, 3 and 5. On the last bar, behind the shut grille, a big round rear silhouette presses against the gate and bounces once: BOING. [2 f] The padlocks jump. It has no detail and no face, and nothing names it. |
| 12 | MT-6 · 02:21-02:26 | Post-chorus to the last chord | The frame splits at the Wall, and the pastel half and the Hellvile half meet on the logo. The Mayor's pink roller crosses the logo from frame 15 (squeak) and paints over its red drip. On the last chord the drip bleeds back through. No sticker. |
| 13 | MT-7 · 02:26-02:30 | After the button | [MUTE 13] Thirteen frames of silence. Then the smallest bubble in the show. |

> **PET**  
> beep. <!-- mt7_pet_01 -->

The last sparkle is purple, and it sinks (21 f). DISSOLVE (13 f) to the Giggle banner. The episode title reads GIGGLE NIGHT, with MINGLE showing through the wet paint. `s_mt7_title`

**Timing.** A locked 50.0 s block. The master runs 50.35 s, and 1-01 absorbs the difference. The only dialogue is one "beep." (raw piezo, `process=none`), gag beep 1 of 6.

---

## ACT ONE, "18:00 Welcome!"
`02:30-07:50 · 5:20 · 14 beats · SC14-SC27 · cap 259 · locked 207 · used 234`

*The act's shape:* the filter at full strength. Four stall gags, a rest, the bandstand, the bake-off, the patrol, Mascotty's page, the speech, an empty sky, and then the first real scare. **No OMINOUS cue until the fog in SC27.** Silence target about 68 s (21%).

### SC14 · 1-01 · ACT CARD / EXT. CANDY HILLS ROAD - SUNSET (18:00)
`02:30-02:48 · 0:18 · elastic ±5 s · cap 0 · used 0`

CARD: the HEE_HEE bell on frame 0. The Mayor's festival clipboard, in the CREAM window skin. His handwriting pops in one line every 8 f: *18:00 Welcome! · 19:00 Bake-Off! · 20:00 Bandstand! · 21:00 (we don't do that) · 22:00 Bedtime!* "18:00 Welcome!" gets a tick. `s_a1_01_card`

[3 s] CUT TO:

The CANDY HILLS at sunset, pastel day melting into pink. A PINK BUS crosses them at a constant speed. No easing: not up the hills, not down them, not round the bends. MUSIC: HaHaland Day (C major, 140 bpm), tinny, from the bus radio.

In the back window: SUNNY (20s, they), a bunnykin with one suitcase and ears that notice everything.

The bus brakes. [SCRATCH] The radio slides down and dies with it. In the silence, Sunny is thrown off the step [DROP, SQUASH 1.22/0.80] and lands sitting in the road.

[8 f] The suitcase lands: BOING, the small sting that breaks the silence. It bounces twice and settles beside Sunny as if it had got there first.

The door folds shut. The bus leaves at exactly the speed it arrived. The dust it leaves behind is pink.

**Timing.** 18.0 s, of which the card takes 3 s. No dialogue. This beat absorbs the main title conform: stretch or trim the bus's crossing, but never the scratch or the BOING.

### SC15 · 1-02 · EXT. TOWN SIGN - SUNSET
`02:48-03:05 · 0:17 · cap 0 · used 0`

A SIGN drops in [DROP, SHK_TAP]: *HaHaland! Population: 500 lovely citizens.* `s_a1_02_sign`

Sunny looks up at it. The last digit rolls over with a cheerful chime: 501. [R8] TINK. The "1" falls off and lands at Sunny's feet. The sign reads 500 again.

Under the 500, if you pause, you can see a 1000 that has been painted over.

[R13] A pencil scratch. Underneath, small, in pencil, someone has added: *and Sunny.* `s_a1_02_pencil` MUSIC: two bars of the HOME motif on the music box, the only score in the scene.

Sunny looks left, then right. Nobody is here. Nobody is holding a pencil.

A pink TICKET flutters down onto Sunny's head [SQUASH 4 f]: *MIDSUMMER GIGGLE, ADMIT ONE (NEW)* `s_a1_02_ticket`

The fallen "1" tips over and rolls away downhill, toward a string of lanterns in the distance. Sunny picks up the suitcase and follows it.

**Timing.** 17.0 s. No dialogue. About 13 s of the scene is unscored, counting toward the act's silence budget.

### SC16 · 1-03 · EXT. GIGGLING FOUNTAIN, PARK GATE - SUNSET
`03:05-03:18 · 0:13 · cap 0 · used 0`

Sunny follows the lanterns to the park gate, under the GIGGLE banner. The cupcake balloon is still tied over the N, still facing front. MUSIC: HaHaland Day and festival walla drift over from the park, filtered by the distance.

Just inside the gate stands the GIGGLING FOUNTAIN: two laughing stone faces, turned away from each other.

Sunny reaches the basin. [R8] The fountain giggles. At nobody. Late. CC: *[the fountain giggles, late]* <!-- lib:fountain/giggle_v1 -->

A soggy page turns over in the water. Sunny fishes it out: *WELCOME SPEECH, p.1.* The ink of every sentence has run. In the margin, wherever a sentence used to be: *Hee. Hee.* `s_a1_03_page1`

Sunny stops still.

[R5] The fountain giggles again, on time this time. CC: *[the fountain giggles, on time: ...Hee.]* <!-- lib:fountain/hee_v1 -->

Sunny takes one careful step back from the fountain.

**Timing.** 13.0 s. No cast dialogue. The fountain's two sounds are performed library takes (about 0.8 s and 0.4 s). This scene and SC05 and SC21 are the only fountain plants. Add none.

### SC17 · 1-04 · EXT. THE MIDSUMMER GIGGLE, HOOK-A-DUCK - DUSK
`03:18-03:31 · 0:13 · Giggle crawl (elastic) · cap 12 · used 10`

THE MIDSUMMER GIGGLE: a pastel fun fair under lantern strings on the north half of Hellvile Park. Candy-striped stalls and a happy walla bed (the Giggle crowd). MUSIC: HaHaland Day, diegetic, from the fair's speakers.

HOOK-A-DUCK: a tub of floating rubber ducks. STEAMY (30s, she), a frogling bath-house keeper in a steam-damp apron, introduces them to Sunny with a hook pole, like a proud mother at a recital.

> **STEAMY**  
> *(proud, quick, even, lilting)*  
> That's Duckworth. <!-- a1_04_steamy_01 -->  
> [15 f]  
> That's Quackers. <!-- a1_04_steamy_02 -->  
> [15 f]  
> That's... <!-- a1_04_steamy_03 -->

[SHOCK, POP] One duck at the back of the tub is facing the Wall.

> **STEAMY**  
> *(a little wobble of alarm)*  
> I don't know that one. <!-- a1_04_steamy_04 -->

[R13] The duck squeaks by itself. Nobody touched it.

[R8] Steamy squeezes Duckworth at it, nervously. Squeak.

[R8] The unknown duck squeaks back: squeak [15 f] squeak [15 f] squeak [R26] ...squeak. Three, then one.

[BUTTON +13: Steamy, very slowly, sinks Duckworth under the water. *Blub.*]

**Timing.** 13.0 s. 10 words, about 3.7 s of speech. The names land 15-16 f apart, so each one is a clipped read of about 0.5 s. Add about 1.5 s for the SHOCK pop, the lone squeak and the answer, and about 3.5 s for the duck's reply and the button, which leaves about 4 s for the walk-up and the tub. **The reply** is the unknown duck's series rule (`GAG_BIBLE.md` §6.18: it squeaks back 3-then-1). Here it quietly plants the Wall's knock before 1-13, and the *blub* comes back when Steamy sinks herself in 3-01. If the crawl is trimmed but this scene stays, cut the reply and the button first. This is the first scene to go if the Giggle crawl is cut.

### SC18 · 1-05 · EXT. THE MIDSUMMER GIGGLE, FACE PAINTING - DUSK
`03:31-03:41 · 0:10 · Giggle crawl (elastic) · cap 12 · used 12`

A sign on an easel: *ANY FACE (within reason)* `s_a1_05_sign`

KEVIN (30s, he), a citizen with three eyes (two where you'd expect them, one in the middle), holds his palette like a shield. Sunny looks. He catches Sunny looking.

> **KEVIN**  
> *(pre-emptive, cheerful-flat)*  
> Yes, three eyes. <!-- a1_05_kevin_01 -->

[R5] Only the third eye blinks. *Tik.*

[R13]

> **KEVIN**  
> *(a touch wistful; don't milk it)*  
> Nobody ever asks about the third one. <!-- a1_05_kevin_02 -->  
> [R8]  
> It's lonely. <!-- a1_05_kevin_03 -->

[BUTTON +13: Sunny blinks back at the third eye with both eyes, which is the best they can do.]

**Timing.** 10.0 s. 12 words, about 4.8 s of speech plus 0.9 s of gaps (R5, R13, R8), so 5.7 s in all, with about 4 s spare. "It's lonely." comes from `GAG_BIBLE.md` §6.14. It is optional, and it is the first thing to cut if the crawl is trimmed.

### SC19 · 1-06 · EXT. THE MIDSUMMER GIGGLE, HATTIE'S TENT - DUSK
`03:41-03:57 · 0:16 · Giggle crawl (elastic) · cap 15 · used 12`

A little striped tent with a crate for a stage. SIGN: *HATTIE HAHA, LIVE! (mostly)* `s_a1_06_sign`. A dozen adults sit on folding chairs.

HATTIE HAHA (30s-40s, she), a plushie comedian, mic-close and unhurried, waits for quiet. She gets it.

> **HATTIE**  
> *(a pro at work)*  
> Knock knock. <!-- a1_06_hattie_01 -->

> **TENT CROWD**  
> *(loose unison, eager)*  
> Who's there? <!-- w_tent_01 -->

> **HATTIE**  
> *(relaxed; let it land)*  
> See, that's the trick. Nobody ever says. <!-- a1_06_hattie_02 -->

> **TENT CROWD**  
> *(a snort-laugh ripple)*  
> (laughter) <!-- w_tent_02 -->

[R8] A second laugh answers from beyond the Wall. It is late, far away, and just one voice.

> **BEYOND THE WALL** (WALLA, DISTANT, FILTERED)  
> *(one laugh, exactly R8 late)*  
> (a laugh) <!-- w_wall_01 -->

CC: *[a laugh from beyond the Wall, late]*

Hattie looks toward it. The music ducks to nothing. [R26]

> **HATTIE**  
> *(tiny, dry, eyes on the Wall)*  
> {d}...Yikes. <!-- a1_06_hattie_03 -->

**Timing.** 16.0 s. 12 words (10 for Hattie, 2 for the crowd), about 5.1 s of speech. Add 1.8 s for the two laughs and 1.1 s for the R8 and R26, which makes about 8 s, with about 8 s spare for the walk-in and her wait. The late laugh lags the ripple by the same 8 f as the fountain: that lag is a series rule.

### SC20 · 1-07 · EXT. THE MIDSUMMER GIGGLE, MISS LULLA'S CRAFT TENT - DUSK
`03:57-04:10 · 0:13 · Giggle crawl (elastic) · cap 12 · used 12`

MISS LULLA'S CRAFT TENT, hushed and warm. MUSIC: the fair's HaHaland Day comes through the canvas, ducked 6 dB. Eleven small children sit drowsily at a low table with crayons in their fists. They stay in the background, in the filter's softest pastels. WALLA: an optional drowsy murmur (the `w_lulla` bed, adults pitched up).

MISS LULLA (30s, she), a sheepkin daycare worker who can nap any baby in under a minute, pegs the last drawing onto a string.

The camera pans along the string at a constant speed. Eleven drawings. Every one of them is the same: a purple cloud wearing a crown. The crayons are different and the hands are different, but the cloud is the same. None of them has a string.

> **MISS LULLA**  
> *(hushed, proud; second half softer)*  
> They all drew the same friend.{..} I don't know who he is. <!-- a1_07_lulla_01 -->

Through the tent flap, across the fair, a sign by the Wall reads *Cloud Nine Vapor, COMING SOON* `s_a1_07_vapor`. It flickers purple. [21 f] It flickers again. It is only a sign: there is no product and no smoke.

Beyond the flap, the crowd bobs past. In the middle of it, Mascotty stands perfectly still.

[NO BUTTON: a rest after four gags.]

**Timing.** 13.0 s. 12 words, about 5.5 s of speech including the R8 inside the line. The constant pan carries the rest. The scene has no punchline, by design.

### SC21 · 1-08 · EXT. BANDSTAND (NORTH HALF) - DUSK
`04:10-04:55 · 0:45 · cap 65 · used 65`

THE BANDSTAND. The north half is lit, hung with lanterns and packed with the Giggle crowd. The south half is boarded up all the way to the Wall. BONEJANGLES (50s, he), Hillvile's skeleton bandleader and xylophone teacher, waits beside the stage with two mallets and his own ribs. MUSIC: HaHaland Day, live from the bandstand.

A SPOTLIGHT sweeps the crowd, overshoots, swings back and lands on Sunny: suitcase, ticket, soggy page and all. Sunny holds perfectly still. It doesn't help. The crowd turns to look [!RIPPLE].

MAYOR GIGGS flaps up onto the rail, wings buzzing, arms wide.

> **MAYOR**  
> *(bright patter, a curtain call)*  
> Welcome, welcome! Mayor Gigglesworth. Friends call me Giggs. Enemies don't exist! <!-- a1_08_mayor_01 -->

The crowd cheers [!RIPPLE].

> **MAYOR**  
> *(leaning in, delighted)*  
> Ooh, a new face! We get so few. <!-- a1_08_mayor_10 -->

He reaches for his speech with a flourish and flips up the schedule on his clipboard. Behind it, where three pages should be, there is only the clip, and air.

> **MAYOR**  
> *(the smile stays; the eyes don't)*  
> My speech. I've given it four hundred times. <!-- a1_08_mayor_02 -->  
> [R8]  
> I can't remember a WORD. <!-- a1_08_mayor_03 -->

Sunny holds up page one. It drips.

> **MAYOR**  
> *(love at first sight)*  
> Page one! Manners! We'll keep you. <!-- a1_08_mayor_04 -->

He takes it in two fingers, clips it on, and spins to the crowd.

> **MAYOR**  
> *(to the crowd, a promise)*  
> The SPEECH is at eight! <!-- a1_08_mayor_05 -->

The crowd cheers. Sunny's eyes drift past him to the boarded half. The camera drifts after them, creeping toward the boards one whole pixel at a time (0.04 px/f): step, step, step.

The Mayor steps into the lens. His smile fills the frame.

> **MAYOR**  
> *(smiling straight into the lens)*  
> Hellvile Park has two sides. This one, and... <!-- a1_08_mayor_06 -->

[FREEZE 13] TAPESTOP: the band's music slides down and dies. His wings stop. He stops mid-breath, mid-smile.

> **MAYOR**  
> *(restarting, too smoothly)*  
> ...the weather side. <!-- a1_08_mayor_07 -->

There is no music.

> **MAYOR**  
> *(flat, no wink: he means it)*  
> {d}We don't talk about the other side. <!-- a1_08_mayor_08 -->

[HOLD 13] Then, [R8] late, far off at the gate, the fountain giggles. CC: *[the fountain giggles, late]*

> **MAYOR**  
> *(two falling notes, same as dawn)*  
> Hee. Hee. <!-- a1_08_mayor_09 -->

Bonejangles slams a FANFARE down his ribs, mallets flying.

> **BONEJANGLES**  
> *(fanfare-happy, two crisp hits)*  
> Clack clack! <!-- a1_08_bone_01 -->

MUSIC: HaHaland Day crashes back in on the downbeat. The crowd cheers as if nothing happened, because, as far as HaHaland is concerned, nothing did.

**Timing.** 45.0 s. 65 words, at the cap. About 24.3 s of speech: the Mayor's patter runs at 2.7 words a second and the deadpan slower. Add 2.1 s of written holds (R8, FREEZE 13, HOLD 13, and the R8 before the giggle), making 26.4 s. The spotlight, the clipboard, the page and the creep take about 12 s, which leaves about 6.6 s spare. The creep needs at least 4 s to read as movement: at 0.04 px/f the camera moves 1 px every 25 f. The new line, `a1_08_mayor_10`, is canon (game Ep 1.1), and it sits in the welcome patter, ahead of the creep, as the breakdown asks. **Script supervisor's check:** at the voice bible's 2.7-3.0 words a second, with the cheers, the clipboard reveal, the page, a 4 s creep, the freeze-deadpan-hold-fountain chain and the fanfare laid end to end, this scene comes out at about 43-45 s of its 45 s. It fits, but it is the tightest scene in the act. If the animatic runs long, cut `a1_08_mayor_10` ("Ooh, a new face! We get so few.") first and let the cheers overlap the welcome; never touch the chain from the creep to "Clack clack!".

### SC22 · 1-09 · EXT. BAKE TENT - NIGHT (19:00)
`04:55-05:50 · 0:55 · bite test elastic (up to -10 s) · cap 75 · used 66`

CLIPBOARD INSERT, held 1 s: the Mayor's pen ticks *19:00 Bake-Off!* `s_a1_09_card`

THE BAKE TENT. Long tables of entries. MUSIC: the NANA motif. The Mayor wears a JUDGE rosette on his bow tie and carries a tiny fork.

NANA PUDDIN's stall has two of everything: two loaves, two cakes, and two pies side by side. The first pie has a name card, NANA. The second has a blank one. Beside them sits a tray of a dozen identical muffins.

NANA PUDDIN (about 70, she), the sheepkin head baker, all wool and flour, wipes her paws on her apron.

The Mayor tastes the first pie. His eyes close.

> **MAYOR**  
> *(judging, transported)*  
> Mm! A-plus! <!-- a1_09_mayor_02 -->

STAMP: an A+ on the first pie's card. His stamp drifts over to the second pie and stops above the blank card.

> **MAYOR**  
> *(lightly, too lightly)*  
> Who's the second one for? <!-- a1_09_mayor_01 -->

> **NANA**  
> *(not looking up)*  
> Habit, sweet pea. <!-- a1_09_nana_01 -->

[R8] He doesn't stamp it. He moves on to the next table, fast.

Nana turns to Sunny and to the tray of muffins.

> **NANA**  
> *(cosy, amused at herself)*  
> The key's in a muffin. Don't ask which. I forgot. I always forget. <!-- a1_09_nana_02 -->

Sunny looks at twelve muffins. Twelve muffins look back.

[3x@15] Sunny bites one. Crunch. Bites another. Crunch, bigger and lower. Bites a third. CRUNCH, bigger and lower again. Nana calls each one by ear as it lands.

> **NANA**  
> *(by ear, like a wine expert)*  
> Raisin. <!-- a1_09_nana_03 -->  
> Raisin. <!-- a1_09_nana_04 -->  
> ...That's a button. <!-- a1_09_nana_05 -->

Sunny takes a button out of their mouth and looks at it. Behind the counter, Nana's paw goes to her cardigan, which is one button short. She carries on as if it isn't. Sunny bites a fourth muffin.

[R13] CRUNCH, the biggest and lowest yet. Sunny's ears go straight up.

[INSERT 4: an x-ray of the muffin. The key inside it has tiny horns. A 4-f tone owns the audio.]

[ONE CHANGE: back on the same shot, Sunny is holding the key. It is small, pink and perfectly ordinary, with no horns.] [+2 f] *Tik.*

> **NANA**  
> *(delighted, a little late)*  
> There it is! Mind your teeth. <!-- a1_09_nana_10 -->

She lifts both pies at once, one on each paw, from long practice. Underneath them, flattened and faintly pie-shaped, is a page.

> **NANA**  
> *(her own read, not his)*  
> Is this yours? It was under the pies. It's mostly "Hee. Hee." <!-- a1_09_nana_06 -->

Sunny takes PAGE TWO. It has a ring of pie juice on it, and down the margin: *Hee. Hee. Hee. Hee.*

Nana reaches under the counter and brings up the basket from the window: the egg in its tea towel, the little lamp, the tag that says *for whoever's next.*

> **NANA**  
> *(softer)*  
> And this. An egg. <!-- a1_09_nana_11 -->  
> It's been waiting for someone. <!-- a1_09_nana_07 -->

She holds it out. Sunny takes hold of it. She doesn't let go. [R13] Then she does.

> **NANA**  
> *(bright, a fib)*  
> The last one hatched into a little demon! Ha! Kidding. <!-- a1_09_nana_08 -->

[R8] MUSIC OUT.

> **NANA**  
> *(the brightness falls out; small)*  
> ...Mostly. <!-- a1_09_nana_09 -->

[BUTTON +13: Nana tucks the tea towel tighter round the egg, the way she did at dawn.]

**Timing.** 55.0 s. 66 words: 49 locked, 5 bite calls and 12 new. About 29.1 s of speech, with Nana at about 2.2 words a second, plus about 1.6 s of gaps (R8 twice, R13 twice, the 4-f insert, the +2 f tik), making 30.7 s. The clipboard, the bites, the pie lift and the egg hand-over take about 13 s, which leaves about 11 s spare. That spare is where the bite test's elastic 10 s lives: the chewing can stretch or trim, but the 15-f bite interval never changes. The new lines are "Mm! A-plus!" (Hillvile grades with letters, ahead of Act Two's ratings out of ten), "There it is! Mind your teeth." and "And this. An egg." (canon, game Ep 1.2). **Script supervisor's check:** counted with every action beat laid end to end, the spare is nearer 5 s than 11 s. That is enough for the 22:00 broadcast cut, but the bite test's full elastic trim (-10 s, Appendix G's 21:00 and 2x11 versions) also needs the three new lines cut (`a1_09_mayor_02`, `a1_09_nana_10`, `a1_09_nana_11`, about 5.5 s with their pick-ups) on top of the chewing.

### SC23 · 1-10 · EXT. FUN FAIR, LANTERN STRING - NIGHT (19:40)
`05:50-06:18 · 0:28 · cap 45 · used 38`

A LANTERN STRING over the fun fair. MUSIC: the PIP motif, pizzicato, with HaHaland Day ducked underneath.

PIP (a child, they), the tiniest bunnykin in town, wearing a JUNIOR FESTIVAL PATROL sash, stands on tiptoe on a sturdy crate and ties something to a lantern with one paw. Under the other arm is MR. FLOPS, a toy bunny with one floppy ear, one not, and a stitched smile.

The thing on the lantern is Sunny's fallen "1". Its tag, in crayon, reads *LOST PROPERTY* `s_a1_10_tag`.

Pip sees Sunny and hops down.

> **PIP**  
> *(fast, official, proud)*  
> Found your one. Lost property goes on a lantern so the owner can see it. <!-- a1_10_pip_01 -->  
> [R8]  
> *(tapping the sash)*  
> Patrol rules. <!-- a1_10_pip_02 -->

Pip salutes. Mr. Flops, held by the arm, salutes too.

> **PIP**  
> *(proud)*  
> I'm the whole patrol. <!-- a1_10_pip_05 -->  
> [R8]  
> *(a glance at Mr. Flops; correcting the record)*  
> And Mr. Flops! <!-- a1_10_pip_06 -->

[R13]

> **PIP**  
> *(hopeful, straight to it)*  
> When's the fireworks? <!-- a1_10_pip_03 -->

The MAYOR sweeps past at speed, clipboard first, in the middle of his rounds.

> **MAYOR**  
> *(passing, brightly)*  
> There's a lovely nine o'clock... <!-- a1_10_mayor_01 -->

[FREEZE 13] He stops mid-stride with one foot in the air. His wings stop.

> **MAYOR**  
> *(restarting, brighter)*  
> ...nothing! <!-- a1_10_mayor_02 -->

He thaws and walks on at twice the speed [PANIC].

INSERT, held 1 s (a held insert, not a whiplash): his clipboard as he passes. *21:00 (we don't do that)* `s_a1_10_card`

> **PIP**  
> *(to Sunny, matter-of-fact)*  
> I saw them once. I was a baby. So it doesn't count. <!-- a1_10_pip_04 -->

[BUTTON +13: Mr. Flops' good ear flops down.]

**Timing.** 28.0 s. 45 words (38 locked, 7 new), about 14.9 s of speech: Pip at 1.3x the cast tempo (about 3.2 words a second), the Mayor at 2.7. Add 2.4 s for the two R8s, the R13, the FREEZE 13 and the 1 s insert, making 17.3 s. That leaves about 10.7 s for the tying, the salute and the Mayor's pass. This scene plants the LOST PROPERTY lantern for 2-03 and the post-credits button. **The new pair** ("I'm the whole patrol." / "And Mr. Flops!", the second one Pip's canon line) plants 4-06: the Mayor's "For the patrol." is a firework for exactly one kid and one toy, and Pip's extra step "for Mr. Flops" pays off "And Mr. Flops!".

### SC24 · 1-11 · EXT. FUN FAIR, EDGE OF THE CROWD - NIGHT (19:55)
`06:18-06:36 · 0:18 · cap 0 (walla only) · used 0`

MUSIC OUT, entirely. Only the crowd's walla is left, close and ordinary (the Giggle bed).

Mascotty stands at the edge of the fun fair, lit by a cool light from nowhere. One paw is closed on the corner of a piece of paper.

Sunny, with the egg in the crook of one arm and the suitcase in the other paw, stops in front of her.

Neither of them moves. [R21] Both idle clocks have stopped. The crowd bobs and chatters round two perfectly still figures, like water round two stones.

Her paw opens [8 f]: PAGE THREE.

Sunny sets down the suitcase and takes it.

She waves, very slowly, at quarter speed. One music-box note.

Sunny waves back with the page, just as slowly.

**Timing.** 18.0 s. No dialogue. The R21 of stillness is the minimum. Hold the two of them for as long as the standoff plays, about 3 s. The paw opens in 8 f and the wave takes about 3 s. It is creepy, then sweet, and the scene has no line at all.

### SC25 · 1-12 · EXT. BANDSTAND - NIGHT (20:00)
`06:36-06:56 · 0:20 · cap 14 · used 10`

THE BANDSTAND. The whole Giggle has gathered. Bonejangles waits with his mallets up.

Sunny hands up pages two and three. The Mayor squares all three like a newsreader, ticks *20:00 Bandstand!* on his clipboard, and taps the mic. THUMP, THUMP.

He reads the whole speech. It is three pages long and one sentence.

> **MAYOR**  
> *(grand, every word a gift)*  
> Welcome to HaHaland, where.{..}.{..}. <!-- a1_12_mayor_01 -->

The three dots type on, 8 f apart, with a page turn under each one: *fwip. fwip. fwip.*

> **MAYOR**  
> *(read off the page, flat)*  
> {d}hee. hee. <!-- a1_12_mayor_02 -->

[HOLD 13]

> **MAYOR**  
> *(beaming, to Sunny)*  
> It kills every time. <!-- a1_12_mayor_03 -->

> **CROWD** (WALLA)  
> *(a ripple of real laughter)*  
> (laughter) <!-- w_giggle_01 -->

It does kill. Bonejangles hits a rimshot on his ribs: ba-dum-CLACK.

The Mayor clips the pages back in behind his schedule. For one second, a fourth sheet shows among them. It is a different colour, pale grey, and it isn't his paper. He does not turn it over.

Bonejangles strikes up the bandstand set: HaHaland Day on his ribs, the hook in C major. The Giggle dances.

**Timing.** 20.0 s. 10 words, about 4.5 s of speech. Add 0.8 s for the dots, the HOLD 13 and about 1.2 s of laughter, making about 7 s. The three page turns land on the three dots, so they cost no time, and they show the joke: three pages for one sentence. The fourth sheet must hold for a full second (30 f), clear of any other action, because it is a rewatch clue. The flat "hee. hee." is the speech gag, not the Mayor's tic: it is a separate read and not the library take.

### SC26 · 1-13 · EXT. THE MIDSUMMER GIGGLE - NIGHT (21:00)
`06:56-07:20 · 0:24 · cap 2 · used 2`

BONG. The town clock strikes nine, bright and quick, and on the first stroke the whole Giggle looks up [!RIPPLE]. Bonejangles' mallets stop in mid-air.

The sky is empty. There are no fireworks, only stars.

[MUTE] The score cuts out, though the sequencer keeps running. The last stroke dies away. [R26] Silence.

One cricket.

[R21] Far off, a balloon pops.

> **MAYOR**  
> *(to the sky, small)*  
> ...Hee. Hee. <!-- a1_13_mayor_01 -->

[INSERT 4: beyond the Wall, a pipe heaves a purple sigh. No faces. A low sigh owns the audio, with no music.]

[ONE CHANGE: back on the same wide, every lantern flame now leans south.] [+2 f] They gutter.

From the Wall: thump. [15 f] Thump. [15 f] Thump. [R26] ...Thump.

At the edge of the crowd, Mascotty lifts one foot and thumps back: thump, thump, thump. [R26] Thump.

Nobody turns. Only Sunny sees.

**Timing.** 24.0 s. 2 words, about 1.1 s: the library take, the tic's third and last use of the episode. The rest of the scene is silence, a cricket, a pop, a sigh and eight thumps. The Wall's 3-then-1 is the rhythm the unknown duck squeaked back in 1-04, at the same spacing. Don't point it out. There is still no OMINOUS cue: the strangest thing so far still gets no music.

### SC27 · 1-14 · EXT. BANDSTAND / THE BOARDS - NIGHT
`07:20-07:50 · 0:30 · ACT OUT · cap 7 · used 7`

Bonejangles starts to play again, alone: the hook, bright, as if that will help. A few people sway along. Nobody looks south.

FOG pours over the boards from the south half, low and lilac. MUSIC: the first OMINOUS cue of the series rises underneath it.

The lanterns go out from the south, one after another, faster and faster: *pff* [21 f] *pff* [13 f] *pff* [8 f] *pff* [5 f] *pff* [3 f] *pff* [2 f] *pff*.

Two red eyes POP open in the fog. *Tik.* [R8] Two more. [R5] Two more. [R3] Two more.

Every eye goes still. [MUTE 40]

Then comes a voice made of many voices.

> **FOG** (WALLA STACK)  
> *(many voices, whispered to half-sung)*  
> {w}come and PLAY with us <!-- w_fog_01 -->

> **CLINKER** (V.O., HER LAYER IN THE STACK)  
> *(same words; flat; slid R8 late)*  
> {w}come and PLAY with us <!-- a1_14_clink_01 -->

[R8] One more voice follows, small and too sweet: a child's.

> **FOG** (A CHILD'S VOICE)  
> *(child-pitched echo, adult performer)*  
> {w}...us <!-- w_fog_02 -->

The fog drifts toward Miss Lulla's craft tent. Its lantern is still lit inside, and the flap is closed.

At his FREE HUGS booth, FATHER FUZZ (40s, he), the plushie bear priest of St. Cuddles, freezes mid-reach, paws out for the next hug.

[INSERT 4: his point of view, in sepia. The kids' tent, eleven years ago tonight. In the doorway stands a cloud-shaped lady with glowing eyes. Behind her is a crowned silhouette, with no string.]

[ONE CHANGE: back on Fuzz. His paws are shaking.]

The fog stops at the tent's guy-ropes. It never reaches the tent. It turns away, toward the music.

Bonejangles plays on, bright. Then the fourth note of his tune bends [13 f] from C to B♭. MUSIC: the HOOK pivots into the RIFF on the E-E-G cell the two tunes share.

The fog pours into his ribcage: the possession inhale. His ribs stretch [10 f] and hold [5 f]. FWOOMP [SHK_FWOOMP].

Sunny pulls the egg in close, tea towel and all.

The last lanterns go out. All of them.

> **ACT OUT (07:50).** One light is left in the park: Bonejangles' ribcage, glowing purple from the inside, like a paper lantern. Everything in frame still bobs, him included. Only his skull moves differently: it turns to camera, slowly [26 f].

> **BONEJANGLES**  
> *(tiny, flat, slightly wrong)*  
> {d}...Clack. <!-- a1_14_bone_01 -->

CUT ON THE CLICK, which is the frame-0 sound of the OUT card.

**Timing.** 30.0 s. 7 words, about 3.4 s of speech: the fog line is slow and stretched at about 2.5 s, the echo 0.4 s and "...Clack." 0.5 s. Add the MUTE 40 (1.3 s), making about 4.7 s. Everything else is picture. The scene has no gag and no button except the act out: it is comic timing with the release removed. The fog never reaches the tent, and no child is ever in a frame with the fog.

---

**END OF ACT ONE**

---

## EYECATCH PAIR 1 (BREAK 1)
`SC28-SC29 · 07:50-08:00 · 0:10 · no dialogue`

### SC28 · EC1-OUT · OUT CARD: THE LANTERN (clock 21:00)
`07:50-07:55 · 0:05`

On frame 0, the act-out click, then the HEE_HEE bell. A paper lantern printed with the logo hangs with the Giggle banner behind it, and the town clock in the corner reads 21:00. Pure HaHaland. A seamless 5 s loop. MUSIC: a 2-bar cut of the theme's hook, in major. `s_ec1_out`

### SC29 · EC1-IN · IN CARD: THE LANTERN, ONE CHANGE
`07:55-08:00 · 0:05`

A 4-f hellify flash at reduced luminance. Then the same card returns with one persistent change: the lantern glows purple from the inside, like a ribcage. Deep in the background, by the banner pole, Mascotty stands, 8 px tall. MUSIC: the hook turned minor (the RIFF), resolving on the ITS_FINE sting. `s_ec1_in`

**Timing.** 10.0 s. No dialogue.

---

## ACT TWO, "21:00 (we don't do that)"
`08:00-12:15 · 4:15 · 8 beats · SC30-SC37`

*The act's shape:* stillness, then the ratings, then panic. Then the hatch in the middle of the panic, the rite's first try, the clue, and the long walk the act ends on. One truth insert (2-03); everything else is behind HaHaland's filter.

**The possessed voice.** Every possessed line is two takes with the same words: Bonejangles (`bone`) and Clinker (`clink`). The engine slides Clinker's take exactly 8 f (267 ms) late, so record her in sync with his take and never lag on purpose. Possessed, Bonejangles speaks in rhyming couplets and says "we", and rates things out of ten: always a whole number, always flat, and a comment after a number is always "Needs X."

### SC30 · 2-01 · ACT CARD / EXT. BANDSTAND - NIGHT
`08:00-08:22 · 0:22 · cap 20 · used 20`

CARD: THE MAYOR'S CLIPBOARD (CREAM window skin), as in Act One, with one change: "21:00 (we don't do that)" is now underlined twice, in a hand that shakes. `s_a2_01_card`

SFX: the HEE_HEE bell on frame 0, as on every act card. It has never sounded less appropriate.

The second underline wobbles, overshoots and stops. [HOLD 2 s]

CUT TO:

The bandstand. Every lantern in the park is out. The only light is BONEJANGLES' ribcage, glowing purple from the inside like a paper lantern.

**FC:** on the first frame of this shot his bob is already 0. Around him the crowd stands frozen mid-party: a bubble wand halfway to someone's mouth, a balloon on a very still paw.

His eye sockets go red: the left one, [R2], then the right. *Tik. Tik.*

The ribs keep playing by themselves: the RIFF in E minor, with no hands on them. His ears and ribs twitch on every frame. Nothing else on him moves.

MUSIC: the RIFF on the ribs becomes the score. Under it, a low OMINOUS bed.

In the crowd, SUNNY holds the egg in its tea towel against their chest. Sunny is the only still thing in the park that is still on purpose.

His jaw does not move. The voice comes out of the ribs, and the purple glow pulses once per syllable.

> **POSSESSED BONEJANGLES / CLINKER**  
> *(dead flat; one syllable per beat)*  
> We came on the fog, and we came for the cake. <!-- a2_01_bone_01 + a2_01_clink_01 (sync late=R8) -->

[R13: one beat of the RIFF]

> **POSSESSED BONEJANGLES / CLINKER**  
> *(same weight on every word)*  
> We came for the party you only half make. <!-- a2_01_bone_02 + a2_01_clink_02 (sync late=R8) -->

The second voice lands on "make" 8 frames after the first, and then there is only the ribs.

[BUTTON +13: something small and wrong. His skull tilts one pixel toward the cake table, with a creak.]

**Timing.** 08:00-08:22 · beat 22.0 s · **dialogue ≈ 10.3 s** (20 words). The couplet runs exactly 6 bars of the RIFF at 140 bpm: 11 beats, a 1-beat rest (≈ R13), 11 beats, a 1-beat rest that carries Clinker's 8-f tail. Layout: card 0-2 s · waking (eyes, ribs, Sunny) 2-7.5 s · couplet 7.5-17.8 s · button and hold 17.8-22 s. Fits.

### SC31 · 2-02 · EXT. BANDSTAND - CONTINUOUS
`08:22-08:55 · 0:33 · cap 16 · used 12`

The skull turns across the crowd one pixel at a time. Wherever it points, someone's breathing stops, in a slow ripple.

In Sunny's arms the tea towel twitches. Once. Nobody sees it.

The skull reaches the cake table at the foot of the bandstand. NANA PUDDIN stands there with a pie. Beside her is MAYOR GIGGS, clipboard clutched to his bow tie, wings stopped dead.

Then, out of pure reflex, Nana cuts a slice and holds it up.

> **NANA**  
> *(by reflex, holding out a slice)*  
> ...Pie, sweet pea? <!-- a2_02_nana_01 -->

The ribs stop. MUSIC: out. [SILENCE]

> **POSSESSED BONEJANGLES / CLINKER**  
> *(a bored verdict)*  
> Six. <!-- a2_02_bone_01 + a2_02_clink_01 (sync late=R8) -->

[R8]

> **POSSESSED BONEJANGLES / CLINKER**  
> *(flatter still)*  
> {d}Needs death. <!-- a2_02_bone_02 + a2_02_clink_02 (sync late=R8) -->

The words just appear: no type-on, no bounce. The slice droops one pixel.

[15 f] The Mayor snatches a cupcake, shakes skull sprinkles over it (*shk-shk-shk*) and holds it out with both hands, hopefully.

> **POSSESSED BONEJANGLES / CLINKER**  
> *(verdict)*  
> Four. Too much death. <!-- a2_02_bone_03 + a2_02_clink_03 (sync late=R8) -->

[15 f] He drops the cupcake, whips out his speech (all three pages) and holds it up, title first: WELCOME SPEECH.

> **POSSESSED BONEJANGLES / CLINKER**  
> *(final)*  
> Two. <!-- a2_02_bone_04 + a2_02_clink_04 (sync late=R8) -->

[R13] The Mayor lowers the speech. Slowly. All the way down.

[BUTTON +13] Nana looks at the Mayor's speech, lowered all the way. Then at her slice. She draws the slice in a little closer.

> **NANA**  
> *(to her slice; quietly winning)*  
> ...Six. <!-- a2_02_nana_02 -->

[R21] Nobody moves. Then one note comes back from the ribs. Then another. The RIFF starts again, slow as a music box winding up, and speeds up with every note. Each note, the crowd's eyes go a little wider [!RIPPLE].

The ribcage swells like a lantern filling with breath: 10-f stretch, then a 5-f hold with a 1-px jitter...

SMASH CUT TO:

**Timing.** 08:22-08:55 · beat 33.0 s · **dialogue ≈ 6.3 s** (12 words, including four 8-f Clinker tails). Layout: the stare-down 0-12 s · Nana's offer and the ratings run 12-24.5 s · the button 24.5-25.5 s · R21, then the RIFF's accelerating return and the swell 25.5-33 s. The ratings run is unscored, from the ribs stopping to their first note back (≈ 11.5 s). Reading of "offers 15-16 f apart": each offer starts 15-16 f after the previous rating ends.

### SC32 · 2-03 · EXT. FUN FAIR - NIGHT
`08:55-09:25 · 0:30 · cap 34 · used 32`

FWOOMP (`SHK_FWOOMP`). The ribcage lets the breath out, and the RIFF crashes back in at swing tempo (dial S2). Every frozen clock in the crowd thaws at once, at double speed. [PANIC ALL]

Panic, on an accelerating curve. Citizens flee in every direction except toward the Wall, each flicking off a sweat drop every third frame.

> **CROWD**  
> *(walla, running)*  
> Fudge! FUDGE! <!-- w_giggle_03 -->

At the cake table the Mayor throws both arms up, beaming the beam of a man in charge.

> **MAYOR**  
> *(arms up, beaming; diving on "under")*  
> Everybody stay calm! I'll be under here. <!-- a2_03_mayor_01 -->

On "under" he dives under the cake table in a smear of wings. The tablecloth drops shut behind him. [R8]

> **MAYOR** (O.S.)  
> *(from under the tablecloth; squeaky)*  
> Being calm! <!-- a2_03_mayor_02 -->

The tablecloth trembles. His halo glows through it.

From between the ribs come PUFF DEMONS. They pop out like popcorn: one, two, then dozens. Each is a fist-sized purple puff with stubby horns, red eyes and far too many teeth. They spill down the bandstand steps into the fun fair. DEMON SQUEAKS.

> **CROWD**  
> *(polite panic)*  
> Single file, please! <!-- w_giggle_04 -->

Nobody goes single file. A citizen trips flat and waves the others on, nobly.

> **CROWD**  
> Leave me! Save the cupcakes! <!-- w_giggle_05 -->

THE TEACUPS. Empty now, still turning, and then turning backwards. TEACUPS WHIRRING IN REVERSE.

> **CROWD**  
> Not the teacups! <!-- w_giggle_06 -->

A Puff Demon bounces into the nearest cup. It gets sucked down, spinning, and sprays out of the top as CONFETTI: pink, gold, one little heart. It's pretty.

[TRUTH 4: on the RIFF downbeat, the same wide shot as it really is. The cup goes through the demon like a blender: a red spiral of fur, meat and teeth whirls up the rim, with one eyeball riding it round. A WET SHRED owns the audio.] **FC:** the wide holds the teacups and the empty fair. No child is in frame, and the red stays inside the cup.

[ONE CHANGE: the inside of that teacup is red, and it stays red.]

[+2 f] *Plip.* One drop. [R8] The nearest Puff Demon peers over the rim into the red cup, and gulps.

MUSIC: the RIFF re-enters on the grid.

THE ABSOLUTELY NOT SCARY HOUSE. SIGN: NOTHING INSIDE! (we checked) —Mayor `s_a2_03_house_sign`. In its windows every cardboard ghost turns to face the Wall [!RIPPLE]. *Fwip-fwip-fwip.*

> **CROWD**  
> Is this the weather?! <!-- w_giggle_07 -->

EXT. BANDSTAND - CONTINUOUS

**FC (standards):** a new `scene=` starts here, at the bandstand. The teacups scene above holds the truth insert and no child (seen or heard); this one holds Pip and no gore, and the red cup is never in it. That way the engine's per-scene `STD-KIDS` lint and the bible's per-frame rule (hard limit 3) both pass.

A GUST of fog slams into the bandstand. The boards over its south half tear loose (CLATTER, CLATTER, CLATTER) and cartwheel off into the dark. Under them is a wooden crate, stencilled MINGLE · DO NOT OPEN, with a small stencilled rocket under the words. `s_a2_03_crate`

In the background of the same wide, with no cut to it, the gust snaps a lantern string. One unlit lantern lifts free and starts to float. It is tagged LOST PROPERTY in crayon, with Sunny's "1" hanging from it.

PIP'S OWN SHOT, framed off the teacups: no teacups, no demons, no fog in it. PIP pops up from behind a hay bale, the Junior Festival Patrol sash askew. They hold MR. FLOPS up so he can see too. Ears straight up.

> **PIP**  
> *(pure joy, full chest)*  
> {big}FIREWORKS!{/} <!-- a2_03_pip_01 -->

A WIDE OF THE STAMPEDE, with no Pip and no teacups in it. Every runner stops mid-stride, plants a foot and turns to face camera-left [!RIPPLE].

> **EVERYONE**  
> *(in unison, exasperated)*  
> NOT NOW, PIP. <!-- w_giggle_02 -->

They set off again at exactly the speed they stopped. In the foreground of the same wide, two citizens reach the gap between two hay bales at the same moment, stop dead and bow to each other. The stampede piles up behind them.

> **CROWD** (ONE CITIZEN)  
> *(very polite)*  
> After you! <!-- w_giggle_08 -->

> **CROWD** (THE OTHER)  
> *(politer)*  
> No, after YOU! <!-- w_giggle_10 -->

Above their heads, in the background, the LOST PROPERTY lantern drifts off into the dark, toward the Wall. Nobody notices. The bow is the foreground gag, so the audience is looking the wrong way too.

The camera cranes up over the stampede and finds the one figure in it who isn't moving: Sunny, in the middle, holding the tea-towel bundle to their chest. The crowd splits around them like water round a stone.

**Timing.** 08:55-09:25 · beat 30.0 s · **dialogue ≈ 12.2 s** (34 words, at the cap: the Mayor 9 words in two files ≈ 3.6 s plus the R8 between them, Pip 0.8 s, the unison 1.0 s, seven walla shouts ≈ 6.8 s laid over the action). Layout: FWOOMP and panic 0-1 s · the Mayor's dive 1-4.8 s · the Puff Demons 4.8-8 s · the teacups and the truth insert 8-13 s · the Not Scary House 13-16 s · the gust, the crate and the lantern 16-20 s · Pip 20-21 s · "NOT NOW, PIP." 21-23 s · the bow in the gap, with the lantern drifting behind it, 23-25 s · the stampede finds Sunny 25-30 s.

### SC33 · 2-04 · EXT. FUN FAIR, THE STAMPEDE - NIGHT
`09:25-10:00 · 0:35 · cap 1 · used 1`

CLOSE ON SUNNY. The stampede streams past behind them. The tea towel slips, and there is the EGG, with this morning's hairline crack.

It wobbles. Every 6 frames: *tock... tock... tock.* Sunny's ears go up.

A Puff Demon bounces off Sunny's shoulder. Sunny doesn't move.

Every 3 frames: *tocktocktocktock.*

On the RIFF's downbeat, it hatches. FLASH (white, 3 f max, reduced luminance). The two halves of shell fall away, and there are two glossy eyes.

[HOLD 21] [MUTE 29] The score drops out. Behind Sunny the chaos goes on in perfect silence: citizens running, demons bouncing, a cupcake tumbling end over end, none of it making a sound.

[f21] The smallest speech bubble in the show.

> **PET**  
> beep. <!-- a2_04_pet_01 -->

CAPTION: [beep.]

[R8, f29] SUPER (it appears; it does not type): ...it's fine. `s_a2_04_fine` MUSIC: the ITS_FINE sting under it.

The nearest Puff Demons flinch [!RIPPLE].

Far off on the bandstand, the possessed bandleader turns. His whole skull rotates toward Sunny, stiff, with no squash. The purple ribs point at the pet like a lamp.

[R13]

> **POSSESSED BONEJANGLES / CLINKER**  
> *(flat, and completely sincere)*  
> ...Ten. <!-- a2_04_bone_01 + a2_04_clink_01 (sync late=R8) -->

MUSIC: the RIFF re-enters on the grid, and the stampede's sound floods back in.

Sunny and the pet look at each other for the first time. Sunny's ears come down around it like a hood. [HOLD 26] No gag.

The pet climbs Sunny's arm in two plush hops and settles on Sunny's head, between the ears. It fits exactly. Without looking down, Sunny scoops the two halves of shell into the tea towel and tucks the bundle under one arm, the way you keep a ticket stub. (They go on the cottage windowsill in T-1.)

Around them the swarm now leaves Sunny a perfect circle of room, like rain round an umbrella. One brave Puff Demon creeps up behind them. The pet turns its head and looks at it. Just looks. The demon backs away very slowly, the way you back away from a bee.

[BUTTON +13: it backs into a hay bale. *Poff.*]

Sunny turns toward the bandstand. Two figures are pushing through the crowd toward its steps.

**Timing.** 09:25-10:00 · beat 35.0 s · **dialogue ≈ 1.2 s** ("beep." ≈ 0.2 s, "...Ten." ≈ 0.7 s plus the 8-f tail). The signature gag: no walla or dialogue over the mute. Layout: the wobbles 0-10 s · the hatch at 10.0 s, with "beep." at +21 f and the caption at +29 f · the flinch, the turn, R13 and "...Ten." 10-14 s · the aftermath 14-35 s: the look, the climb and the circle of room with no scripted gag for about 9 s, then the brave demon, the button and the turn to the bandstand.

### SC34 · 2-05 · EXT. BANDSTAND STEPS - NIGHT
`10:00-10:28 · 0:28 · cap 42 · used 37`

The crowd has regrouped at a safe distance: behind hay bales, behind the cake table, behind each other. The cake table's tablecloth glows faintly, in the shape of a halo. Behind the table is Nana, and behind Nana are Pip and Mr. Flops. Pip stays behind an adult and well out of reach for the rest of the act, and the fog keeps to the fair, away from the steps.

FATHER FUZZ, plush bear priest of St. Cuddles, comes through the crowd at a youth-group leader's trot, a satchel bouncing on his hip.

> **FUZZ**  
> *(trotting; cheerful, urgent)*  
> Pardon me! Clergy! Coming through! <!-- a2_05_fuzz_03 -->

Citizens step aside for him, and several hug him on the way past. He hugs each one back without breaking stride. DR. BLOOM follows at a walk: a ghoul in pastel scrubs.

> **BLOOM**  
> *(behind him, flat)*  
> Doctor. <!-- a2_05_bloom_03 -->

The crowd parts for her instantly, and a little further than it needs to.

They reach the steps. Sunny arrives at the bottom, the pet on their head. Up on the bandstand, possessed Bonejangles stands as he has all act: dead still, ears twitching every frame, the ribs playing the RIFF on their own.

Bloom climbs, takes his wrist bone between two fingers, and holds up one finger of her other hand at the ribs.

The ribs stop mid-bar. MUSIC: the RIFF ducks out.

Silence. She checks a pastel fob watch. *Tik. Tik. Tik. Tik.* Her lips move as she counts. Fuzz waits. The crowd waits. Two red eye sockets slide down to look at her hand. She does not look up.

> **BLOOM**  
> *(clinical)*  
> No pulse. <!-- a2_05_bloom_01 -->

[R8]

> **BLOOM**  
> *(dry; the tiniest smile)*  
> He never has one. <!-- a2_05_bloom_02 -->

She lets go. The ribs take up the RIFF again, sulkily, from where they stopped.

Fuzz kneels at the rail and takes three candles from his satchel. He sets them down 15 f apart, each one landing with a TINK and a `SHK_TAP` [3x@15]. Three small gold flames, the only warm light in the park.

MUSIC: the FUZZ hymn motif enters, small, under the RIFF.

> **FUZZ**  
> *(steady, teaching)*  
> Three parts. The rhythm, the water, the name. Three tries. <!-- a2_05_fuzz_01 -->

On "Three tries." he nods at each candle in turn. Then he rolls up his plush sleeves.

> **FUZZ**  
> *(gentle; means every word)*  
> A demon is a frightened thing in a loud coat. You take the coat off. <!-- a2_05_fuzz_02 -->

This is played straight. The candlelight holds on his face. Nobody laughs.

[BUTTON +13: the ribs answer him with three lazy notes in minor, stepping down: the musical version of a yawn.]

**Timing.** 10:00-10:28 · beat 28.0 s · **dialogue ≈ 13.6 s** (37 words at ≈ 2.6 w/s; Bloom slower). Layout: the entrance 0-3.5 s · the wrist 3.5-4.5 s · the pulse check, unscored, 4.5-11.5 s (Bloom's two lines fall at the end of it) · the candles 12-13.5 s · "Three parts..." 13.5-17.3 s · the sleeves 17.3-18.3 s · the creed 18.3-24 s · the button and a hold 24-28 s.

### SC35 · 2-06 · EXT. BANDSTAND - NIGHT
`10:28-11:08 · 0:40 · cap 12 spoken (+ a sung phrase) · used 10 (+ 12 sung)`

TRY 1.

Fuzz plants himself at the top of the steps, square to the possessed, the candles at his back. Arms flung wide: a bear about to hug a building. Bloom stands a step below.

> **FUZZ**  
> *(full chest, arms wide, no wink)*  
> THE POWER OF CUDDLES COMPELS YOU! <!-- a2_06_fuzz_01 -->

It types out big, every "!" flashing every 3 f. The score ducks under him.

[R13]

> **POSSESSED BONEJANGLES / CLINKER**  
> *(tiny, instant)*  
> ...One. <!-- a2_06_bone_01 + a2_06_clink_01 (sync late=R8) -->

The words are the size of a crumb.

[R8] Fuzz's arms stay wide, and so does his smile.

> **FUZZ**  
> *(undimmed)*  
> One's a start! <!-- a2_06_fuzz_03 -->

THE RHYTHM. Fuzz claps on two and four, on the 140 bpm grid, and sings the first phrase of the St. Cuddles hymn, bright and in major. He claps alone. Nobody in the crowd joins in.

> **FUZZ**  
> *(SUNG: major; clapping on 2, 4)*  
> Come in close, come in close,  
> hold the one who's frightened most. <!-- a2_06_fuzz_02 -->

Bar by bar, the ribs answer him with the same notes in minor, a hair behind, and the gap keeps growing:

* **Bar 1:** "Come in close," on the grid. The ribs echo it in minor.
* **Bar 2:** "come in close," 2 f late. His claps start chasing the ribs.
* **Bar 3:** "hold the one who's," 5 f late. His ears pin back. He sings louder. It doesn't help.
* **Bar 4:** "frightened most," 8 f late, and it comes out of him in minor. He hears it, and his ears go down.

The ribs play the phrase once more on their own, perfectly, in minor, to show him how it's done.

KAZOO: three notes, 15 f apart, stepping down [3x@15].

The first candle goes out with a red flash (local to the candle, 3 f max, reduced saturation) and a thread of smoke.

FAIL x1 SLAMs in from 2.0x. `SHK_STAMP`. The FAIL sting. `s_a2_06_fail`

[BUTTON +13: under the cake table, a plump little hand reaches out and pulls the tablecloth down another inch.]

Fuzz looks at the dead candle. His paws, still mid-clap, are shaking, the way they shook at his FREE HUGS booth. He presses them together, flat, until they stop. Bloom sees it and says nothing. [HOLD 21]

Two gold flames are left on the rail. The ribs pick the RIFF back up, easy, like a victory lap, as the camera drifts down to the boards at the bandleader's feet.

**Timing.** 10:28-11:08 · beat 40.0 s · **spoken dialogue ≈ 4.2 s** (10 words: the battle cry ≈ 2.4 s, "...One." ≈ 0.9 s with its tail, "One's a start!" ≈ 0.9 s) **plus the sung phrase ≈ 6.9 s** (4 bars at 140 bpm, inside the duel). Layout: the stance 0-2 s · the cry, the rating and the comeback 2-7 s · the duel (4 sung bars, 4 answered bars and the ribs' solo bar) 7-22.5 s · the kazoo, the candle and FAIL 22.5-25.5 s · the button 26 s · the paws 27-34 s · the victory lap 34-40 s.

### SC36 · 2-07 · EXT. BANDSTAND BOARDS - NIGHT
`11:08-11:48 · 0:40 · cap 53 · used 43`

The ribs stop. MUSIC: out. There is no score while the bones write: only the chalk.

Possessed Bonejangles folds down to the boards in one stiff hinge, like a ruler. One finger bone touches the wood, and chalk dust falls from it. He writes at a constant speed, with no easing, and the camera reads it over his shoulder.

CHALK, on the boards (written, never spoken aloud): `s_a2_07_poem`

*Down by the Wall where the pumps go clink,*

SFX: CHALK SCRAPE. After "clink", from far off beyond the Wall, a faint, real *clink* answers. CAPTION: [a faint clink, beyond the Wall]

*we work all night, and we-*

The chalk stops mid-word.

[STILL 13] Every idle clock in frame stops, even the twitching.

> **POSSESSED BONEJANGLES / CLINKER**  
> *(rating its own poem; unimpressed)*  
> ...Three. Needs an ending. <!-- a2_07_bone_01 + a2_07_clink_01 (sync late=R8) -->

MUSIC: the low OMINOUS bed creeps back in under them.

Behind Nana's apron at the foot of the steps, Pip leans out, ears turned toward the Wall. Mr. Flops leans out too.

> **PIP**  
> *(quick, quiet, very sure)*  
> It keeps saying clink. <!-- a2_07_pip_01 -->

Nana is looking at the chalk the way she looks at a cake that's sunk.

> **NANA**  
> *(slow; remembering)*  
> I know that rating. Last Mingle, somebody gave my pie a six. <!-- a2_07_nana_01 -->

[R8]

> **NANA**  
> *(the warmth drops out)*  
> Said it needed death. <!-- a2_07_nana_02 -->

Fuzz looks at the two candles left, and the light comes back into his face.

> **FUZZ**  
> *(quick; hope rising)*  
> Then the name's from that night. <!-- a2_07_fuzz_01 -->

> **NANA**  
> *(steady, sure)*  
> Everybody who came that night is in my recipe tin. <!-- a2_07_nana_03 -->

The tablecloth lifts an inch: two eyes and a moustache.

> **MAYOR**  
> *(from under the table; small)*  
> ...Where's the tin? <!-- a2_07_mayor_01 -->

Everyone turns [!RIPPLE] toward the far side of the fair.

MUSIC: the RIFF returns, low.

PUSH IN across the whole dark fun fair to the BAKE TENT on the far side, its bunting hanging limp. Between here and there, in the fog, in the teacups and in the dark under every stall, are red eyes. They blink pair by pair at R8 intervals, like a string of lights coming on wrong. **FC (standards):** the push-in starts past the group, so no child is in it; the red teacup's inside stays dark (eyes only).

[NO BUTTON]

**Timing.** 11:08-11:48 · beat 40.0 s · **dialogue ≈ 18.5 s** (43 words, all locked; Nana at ≈ 2.2 w/s, Pip at 1.3x tempo). Layout: the ribs stop 0-1 s · the chalk, unscored, 1-12 s (66 characters at a constant ≈ 6 a second; the Wall's clink at ≈ 6.5 s) · STILL 13 12-12.4 s · the rating 12.4-14.9 s · the run of talk from Pip's line to the Mayor's, with its looks, 14.9-34.0 s · the turn 34.0-34.6 s · the push-in 34.6-40 s. Slack ≈ 0.5 s. **Script supervisor's trim:** the added exchange "The same voice?" / "Same opinions." (`a2_07_fuzz_02`, `a2_07_nana_04`, 5 words, ≈ 2.6 s with pick-ups) was cut: with it the beat ran about 3 s over its 40 s. Restore it only if the animatic plays the chalk faster than 6 characters a second.

### SC37 · 2-08 · EXT. WHACK-AN-IMP BOOTH - NIGHT
`11:48-12:15 · 0:27 · cap 0 · used 0 (plus "beep.")`

Sunny stands at the edge of the bandstand, the pet on their head. Sunny looks at the tent. [R8] At the eyes. [R8] Back at the tent.

Sunny steps down off the bandstand, into the dark.

Halfway there stands the WHACK-AN-IMP booth. As Sunny passes, its plastic imps pop up out of their holes [!RIPPLE], see the fog, and pull their lids shut over their heads, one after another, 2 f apart. *Clack clack clack clack clack.* One lid lifts a crack, sees Sunny, and shuts again. *Clack.*

On the counter, under its own caged work lamp (the one light on this side of the fair that still works), sits RIVET'S PRIZE CASE: welded steel, rivets the size of buttons, and a lid you could stand on.

MUSIC: the RIFF drops out.

The camera tilts down into the case (13-f smoothstep). On purple velvet lie three prizes, each with a tag, and each tag holds for its reading time:

* A BUBBLE WAND. Its tag says BUBBLE WAND. `s_a2_08_tag_wand`
* The PUMP SHOTGUN: a bath-house bellows, a length of drainpipe and a bike bell, welded into something you would not want pointed at you. Its tag, PARTY PUMP, is pinned over an older one that reads SLAUGHTER. `s_a2_08_tag_pump`
* A WELDED SAW. PURRING SAW, pinned over MEATCLEAVER. `s_a2_08_tag_saw`

A SIGN is taped to the glass in a blunt hand: TOP PRIZE. Please don't. —R `s_a2_08_sign`

MUSIC: the RIVET motif (G mixolydian, plus an anvil hit), a 2-bar sting under the sign. Then silence.

Sunny looks at the case. At the tent. At the case.

Sunny's paw goes onto the glass: a 6-f pull back, a 3-f press and an 8-f settle, the pad squashing flat on the contact frame [SQUASH 4 f].

[R13]

On Sunny's head, the pet looks down at the prizes.

> **PET**  
> beep. <!-- a2_08_pet_01 -->

CAPTION: [beep.]

> **ACT OUT (12:15).** One candle is out and two are left. The only name that might work is in a tin across a fair full of red eyes. A paw is on the glass. "beep." The glass cracks on frame 0 of the OUT card.

**Timing.** 11:48-12:15 · beat 27.0 s · **dialogue ≈ 0.2 s** ("beep."). Layout: the looks 0-3 s · the walk and the imps 3-8 s · the case, with the RIFF out, 8-10 s · the three tags 10-17 s (the wand 1.5 s, the pump 2.8 s, the saw 2.7 s) · the sign and the RIVET sting 17-20.5 s · silence, the looks and the paw 20.5-24.5 s · R13 · "beep." at ≈ 25 s · hold to the cut at 27.0 s. Unscored ≈ 15 s.

---

**END OF ACT TWO**

---

## EYECATCH PAIR 2 (BREAK 2)
`SC38-SC39 · 12:15-12:25 · 0:10 · no dialogue`

### SC38 · EC2-OUT · OUT CARD: THE EGGSHELLS (CLOCK 21:40)
`12:15-12:20 · 0:05`

SFX: on frame 0, the prize-case glass CRACKS (the act out's sound). Then the HEE_HEE bell.

Two eggshell halves hang from the lantern string like two tiny lanterns. Between them is a tiny "beep." bubble: text only, no sound. `s_ec2out_bubble` In the corner the town clock reads 21:40. Pure HaHaland. A seamless 5 s loop.

MUSIC: the theme's hook in major, 2 bars.

### SC39 · EC2-IN · IN CARD: THE EGGSHELLS, ONE CHANGE
`12:20-12:25 · 0:05`

[FLASH: hellify, 4 f, reduced luminance, Harding-checked]

The same card, with exactly one change: a tiny Puff Demon sits in the second shell, where the pet would be, and beeps, lower. SFX: the piezo beep pitched down, a demon's copy. It is a sound effect, not one of the pet's gag beeps. CAPTION: [a lower beep]

MASCOTTY stands in the deep background, 8 px tall, at the same depth as in EC1-IN.

MUSIC: the RIFF, resolving on the ITS_FINE sting.

---

## ACT THREE, "22:00 ~~Bedtime!~~ weather"
`12:25-16:40 · 4:15 · 7 beats · SC40-SC46`

*The act's shape:* filter, filter, truth (a rule of three across 3-01, 3-02 and 3-03); the whiplash to silence on the steps (protected hold 1 of 3); the truth again, and the REWIND; then the tin, and the name nobody can say. This is the gore act: two truth inserts and 70 s of first person, all of it on demons, with no child in any of those frames. First person has no voice in it. In the two first-person scenes, `[~f260]` is a frame count from the start of that scene at 30 fps.

*Carried over from Act Two:* the Mayor has been under the cake table at the foot of the bandstand since 2-03; the inside of one teacup is red; Rivet's prize case stands open, its glass cracked; the chalk poem is still on the boards; two of Fuzz's three candles are lit; Fuzz's satchel holds the holy water.

### SC40 · 3-01 · ACT CARD / EXT. FUN FAIR - NIGHT (CUTE MODE)
`12:25-13:15 · 0:50 · elastic ±20 s · cap 6 · used 6`

CARD: THE MAYOR'S CLIPBOARD (CREAM window skin), the same page as Act One's. The pen stops on the last line, *22:00 Bedtime!*, and crosses out *Bedtime!*: one hard stroke, [R5] then two more. Above it, in the same shaking hand, smaller, as if it doesn't want to be overheard: *weather*. `s_a3_01_card`

SFX: the HEE_HEE bell on frame 0. The pen scratches.

[HOLD 1 s]

CUT TO:

POP: the first bubble, on the HOOK's downbeat (the cut's frame-0 sound).

THE FUN FAIR, CUTE MODE: a flat side view, like a platform game. Every lantern has been dark since nine, but HaHaland's filter makes even this cute: the dark is a soft lilac, and the PUFF DEMONS bouncing between the stalls are round as gumdrops. Dozens of them. Red eyes, far too many teeth.

MUSIC: the cute arrangement of the HOOK (dial S1). Glockenspiel and music box, much too sweet for the situation.

SUNNY marches in from screen-left, the pet riding on their head between the ears. Behind them, Rivet's prize case stands open, its glass cracked, the velvet empty. In Sunny's paw: the BUBBLE WAND. Slung across their back on a strap: the PUMP SHOTGUN and the WELDED SAW, tags fluttering. PARTY PUMP. PURRING SAW.

Every bubble the wand makes glows a little, like a lamp. Sunny is bringing the only light.

**WAVE 1** (4 bars). Demons squeak out from under a stall. Sunny swings the wand on the beat, like a conductor. A stream of bubbles, and each one swallows a demon whole. Inside, the demons press their faces to the film, offended. The bubbles float up. On every second beat one POPs (hit-stop 2 f, a POP shake, SPARKLE) into a cartoon heart and a puff of plush stuffing, which drift down like snow. The pet nods along on every pop.

Bar 3, on a pop's beat:

[TRUTH 4: the same bubble as it really is. The demon bursts like a water balloon, and out of the burst drops an anatomical heart, still beating. A WET POP owns the audio.] **FC:** no child in frame. The red stays inside the burst.

[ONE CHANGE: on the grass, among the cartoon hearts, one is beating.] [+2 f] *ba-dum.* [R8] The pet looks down at it.

Sunny steps carefully over it and keeps marching. `[ELASTIC: wave tail]`

**WAVE 2** (4 bars). Past the TEACUPS, still turning backwards. The red one is still red. Demons spill out of the cups at Sunny. Bubble, bubble, bubble. Pop, pop, pop, on the beat.

On a pop's beat:

[TRUTH 4: a Bubble Wand hit as it really is. The film snaps tight round the demon like cling wrap and keeps on shrinking. The bubble fills with red, and splits. A WET POP owns the audio.] **FC:** no child in frame. The red stays inside the bubble.

[ONE CHANGE: the wand's next bubbles come out faintly pink.] [+2 f] *blip.* [R8] Sunny looks at the wand. Blows one more. Pink. Sunny carries on.

THE HOOK-A-DUCK, the tub from 1-04. At the back, the duck nobody knows still faces the Wall. In the middle, up to her eyes in the water, STEAMY is hiding, a rubber duck balanced on her head for camouflage.

A bubbled demon drifts across the tub at exactly hook height.

Steamy's hook pole rises out of the water by pure reflex and catches the bubble by one stubby horn, the way you'd hook a duck by its ring. She lifts it to eye level. The demon looks at her through the film. She looks at the demon. [R13]

> **STEAMY**  
> *(the same little tune, smaller)*  
> ...I don't know that one either. <!-- a3_01_steamy_01 -->

[BUTTON +13: the bubble pops in her face: one heart, one puff of stuffing. She sinks slowly back under until only the duck on her head shows. *Blub.*]

**WAVE 3** (4 bars) `[ELASTIC: the whole wave]`. The thickest swarm yet, between Sunny and the far side of the fair. Sunny conducts it: a blizzard of bubbles, a pop on every beat, hearts and stuffing coming down like a snow globe shaken hard. The last demons break and flee, squeaking, toward the BAKE TENT, bouncing over each other to get there first.

`[ELASTIC: tail]` The camera trucks right with Sunny through the falling hearts, past Fuzz's FREE HUGS booth, empty, its sign hanging by one corner. Ahead, the demons are piling onto the bake tent's roof.

**Timing.** 12:25-13:15 · beat 50.0 s · **dialogue ≈ 2.5 s** (6 words).
* Layout: card 0-2.5 s · entrance 2.5-6 s · wave 1 6-12.9 s (4 bars at 140 bpm; insert at ≈ 10.3 s) · its tail 12.9-16.3 s · wave 2 16.3-23.2 s (insert at ≈ 20.6 s) · Steamy 23.2-30.5 s (the line at ≈ 26.6 s, the button at ≈ 29.6 s) · wave 3 30.5-37.4 s · tail 37.4-50 s.
* **The trim** takes wave 3 (6.9 s) and up to 13 s of the tails, so the shortest version is 30 s. Both inserts and Steamy survive every cut.
* The only joke is Steamy. The inserts' returns are the switch, not gags.

### SC41 · 3-02 · EXT. BAKE TENT - NIGHT
`13:15-13:30 · 0:15 · cap 0 · used 0 (plus "beep.")`

SQUELCH on frame 0.

THE BAKE TENT. Over the flap, in bunting letters: BAKE TENT. `s_a3_02_tent_sign`

On the roof the fleeing demons pile into a tower, one on top of another (*squelch, squelch, squelch*), and run together like wet dough into ONE BIG PUFF, three times the size, perched on the ridge pole. The canvas sags under it. Hell drips and never wobbles: it doesn't bounce once. A rope of drool lowers from its mouth to the grass. *Plap.*

Two red eyes the size of pie dishes roll down to Sunny.

Sunny blows a bubble at it. It bounces off. *Bwip.* [15 f] A bigger one. *Bwoop.* [15 f] The biggest yet. *BWOMP.* [3x@15] The Big Puff doesn't so much as wobble. The third bubble sails away over the fair and pops somewhere far off, very small. *pip.*

The look chain, R8 apart:
* Sunny looks over their shoulder at the Pump Shotgun.
* [R8] The pet leans forward over Sunny's brow and looks at Sunny, upside down.
* [R8] Sunny looks at the camera. [HOLD 8: into the lens]

> **PET**  
> beep. <!-- a3_02_pet_01 -->

CAPTION: [beep.]

Sunny drops the wand. The shotgun swings round in a 3-f smear into both paws. Sunny pumps it.

SFX: *SQUEAK*: a rubber-duck squeak out of the bath-house bellows. It is the filter's sound, and the last cute sound in the act. CAPTION: [squeak]

[SCRATCH] The camera brakes (`TW_BRAKE`, `SHK_BRAKE`). The HOOK tape-stops over 5 f. Then 14 f of silence.

Sunny pumps it again.

SFX: CHK—

SMASH CUT TO: (in the gap between the two halves of the CHK-CHK)

**Timing.** 13:15-13:30 · beat 15.0 s · **dialogue ≈ 0.2 s** ("beep.").
* Layout: the stack 0-6 s · three bubbles 6-8 s (15 f apart) · the eyes and the drool 8-9.5 s · the look chain 9.5-11.3 s · "beep." at ≈ 11.5 s · the wand drops and the shotgun swings round 11.8-13.6 s · squeak at 13.6 s · SCRATCH and silence 13.7-14.4 s · CHK— at ≈ 14.7 s, and the cut at 15.0 s lands between the two halves of the sound.
* This scene plays the act's one SCRATCH. 3-03 opens on the second half of the CHK-CHK, so the scratch isn't heard twice.

### SC42 · 3-03 · INT. THE ABSOLUTELY NOT SCARY HOUSE - NIGHT (FIRST PERSON)
`13:30-14:10 · 0:40 (1,200 f) · first person · cap 0 · used 0`

FIRST PERSON. For 40 seconds we are Sunny's eyes. (This is the trailer's s10 raycaster, re-staged for 16:9.)

—CHK, on frame 0: the truth's half of the pump. On it comes the hell slide (chromatic, falling) as HELLIFY rots the palette pixel by pixel over 21 f, from pastel down into the HELL and BLOOD ramps.

Ahead is the front of THE ABSOLUTELY NOT SCARY HOUSE, a smiling cartoon ghost painted over the door. The sign: NOTHING INSIDE! (we checked) —Mayor `s_a2_03_house_sign`. The rot crawls across its letters while we read it. [≈ 2 s]

The barrel of the Pump Shotgun rises into the bottom of frame. The door creaks open on dark.

MUSIC [~f39]: the insert song: "Doomtastic Times", TV size, in its DOOM-side arrangement. It comes in on the last two bars of the pre-chorus, "Knock, knock, knock! Come out!", sung sweetly over detuned guitars, as the door creaks open.

[~f135] [SLAM] On the chorus's downbeat THE HUD slams up into the bottom of frame, a 32-px strip:
* hearts on the left: three;
* shells on the right: eight;
* in the centre slot, the pet's face in LCD green, glancing left, right, left;
* the weapon plate: PARTY PUMP;
* under it, a tiny plate: RIP AND TEAR... GENTLY;
* and the mode label: GORE: BRUTAL. `s_a3_03_hud`

[HOLD 8] GORE: BRUTAL sits there in red.

[SLAM, stamp: 4 f] A round sticker slaps over it: a doily edge, a smiling rubber stamp, HELLVILE PARK BOARD OF DECENCY (approved). `s_a3_03_sticker` The pet's face looks down at it.

[13 f] A Puff Demon pops up from behind a cardboard tombstone, squeaking, all teeth.

[~f160] BLAM. [HIT-STOP 2] `SHK_SHOTGUN`. The muzzle flash stays on the barrel. The demon comes apart: red mist, gibs, a loose eyeball. Inside the blast there's a paper RIP: the sticker tears off the HUD in a spray of red, flutters down the lens end over end and drops out of frame. On the same frame the plate flips: PARTY PUMP → SLAUGHTER. The label underneath still says GORE: BRUTAL.

The pet's face grins.

[BUTTON: the eyeball rolls to a stop, and its pupil turns, *tik*, toward the Wall.]

CHK-CHK.

**WAVE 1: THE CORRIDOR OF CARDBOARD GHOSTS.** Cardboard ghosts swing on wires, smiling, harmless. Real Puff Demons hide between them.
* One lunges. BLAM: it smears down a cardboard ghost's smile. A tooth *tinks* off the lens. CHK-CHK.
* Two come at once. BLAM: one red bloom, and a horn clonks into an abandoned popcorn bucket. *Clonk.* CHK-CHK.
* Splatter slides down the lens on twos and is gone before the next hit. (Shells: 5.)

**BETWEEN WAVES.** A cardboard ghost swings round on its wire into the sights. BLAM, by reflex. It just falls over flat. *Fwap.* No red at all: it's cardboard. The pet's face winces.

[R13] It has landed face down. Painted on its back: THANK YOU FOR NOT SCREAMING —Mayor `s_a3_03_ghost_back` (Shells: 4.)

**THE WALK.** Ahead: six painted doors, all shut, each with a brass knocker. On door one the knocker lifts by itself: *knock* [8 f] *knock*. The song ducks under it. CC: *[knock, knock]* [R13] Nobody says who's there. (Hattie's 1-06 joke, answered.)

**WAVE 2: THE ROOM OF DOORS.** MUSIC: the post-chorus (the stab riff, the HOOK on twin leads, a gang shout of "Hai!"). The doors bang open in a ripple, and a demon bursts out of every one.
* BLAM: door one. CHK-CHK.
* BLAM: door four, and an eyeball rolls under door five. CHK-CHK.
* Door six gets to the lens first. SPLAT: demon blood across the glass, and a heart pops off the HUD (three to two). The pet's face winces. The red slides off on twos.
* BLAM: door six, and most of the door with it. (Shells: 1.)

MUSIC: the post-chorus's own stab tears on the record and tape-stops.

**BETWEEN WAVES**, in the gap: on a little plinth with a doily sits a box of shells labelled PARTY POPPERS. Pinned under that label is an older one: SHELLS. `s_a3_03_ammo_box` Sunny's paw takes it. (Shells: 6.) The pet's face grins.

**WAVE 3: THE COSTUME ROOM.** MUSIC: the Hellvile riff on DOOM guitars, instrumental only. *Oi!* Rails of white bedsheet-ghost costumes on hangers run all the way to the back wall, and a Puff Demon's red eyes show through the eyeholes of every one.
* BLAM: a red bloom spreads through a white sheet. CHK-CHK.
* BLAM. BLAM. Sheets shred, and eyeballs roll out from under the hems.
* Two sheets lunge together. BLAM: one red bloom through both. CHK-CHK.
* The last one. BLAM: a gib slaps the lens and slides off. (Shells: 1.)

MUSIC [~f999]: the second chorus, from its downbeat.

THE BACK DOOR: EXIT. Through it, a strip of night and the back wall of the bake tent, its canvas lit from inside. On the canvas is a shadow that fills it: the Big Puff is inside now, and so is the tin.

The barrel comes up. One shell on the HUD. CHK-CHK.

MUSIC: "...Say it out—"

WHIPLASH CUT TO:

**Timing.** 13:30-14:10 · beat 40.0 s (1,200 f) · **dialogue 0 s**.
* Layout: hellify and the sign f0-75 · the door f75-135 · HUD, sticker and first blast f135-190 · wave 1 f190-340 · the cardboard ghost and its back f340-450 (the sign reads for 2.3 s) · the walk and the knock-knock f450-519 (knocks at about f475 and f483, then R13 of nobody answering) · wave 2 f519-711 · the pickup f711-770 · wave 3 f770-999 · the exit and the shadow f999-1,200.
* **Blasts:** 12 in about 28 s, never more than 2 a second (the cap is 3). Every kill has a sound on frame 0 and a gore button. The lens is clean before each wave's first hit.
* **Music, suggested conform** (150 bpm, 12 f a beat; check against the DOOM-side stem when it lands):
  * f39: the pre-chorus's last 2 bars;
  * f135: chorus 1, with the HUD slam on its downbeat;
  * f519: the post-chorus, ending in its own tape-stop at f711;
  * f711: 6 bars of the Hellvile riff, **instrumental only** (the bridge's riff bars, looped; never into the bridge vocal, "The Wall said 'Boo!'...");
  * f999: chorus 2;
  * f1,200: the hard cut, three frames after the vocal's "out".
* The vocal is cut on "Say it out—". The act's question is who will say it, and the song is cut off on the word.
* First person: 40 s, plus 30 s in 3-05, which is 70 s against the 75 s cap.

### SC43 · 3-04 · EXT. BANDSTAND - NIGHT
`14:10-14:50 · 0:40 · PROTECTED HOLD 1 of 3 · cap 22 · used 20`

[SILENCE] The insert song is gone in the middle of a word. Its sequencer runs on under the mute, so it can come back on the grid in 3-05.

Frame 0: one bandstand board CREAKS under a plush foot. It's the only sound.

THE BANDSTAND STEPS, wide. On the rail are two gold flames and one dead wick. At the top, possessed BONEJANGLES stands where he has stood all night: dead still, ears and ribs twitching on every frame, purple light leaking out between his ribs. The ribs are silent.

On the bottom step, FATHER FUZZ is on his way up, paws already open. DR. BLOOM catches his sleeve.

> **BLOOM**  
> *(quiet, urgent; hand on his sleeve)*  
> Father, don't. <!-- a3_04_bloom_01 -->

[R8] He looks at her hand and pats it, once.

> **FUZZ**  
> *(gently; certain)*  
> Nothing is unhuggable. <!-- a3_04_fuzz_01 -->

She lets go.

He climbs, one step at a time. *pff. pff. pff.* The camera holds wide and doesn't follow.

Behind the hay bales the crowd watches him go, and one after another they forget to breathe [!RIPPLE]. Behind Nana's apron, Pip puts a paw over Mr. Flops' eyes and keeps their own open. Under the cake table the tablecloth lifts an inch: the Mayor's eyes, not blinking.

The red sockets follow Fuzz all the way up.

At the top he stops, one step below the bones, and opens his arms. [R13] Then he steps in and hugs the possessed bandleader the way he hugs everybody: all the way, eyes shut, cheek against the ribs.

CONTACT FRAME. Fuzz's hugs always squash the one being hugged. This one doesn't squash at all. The ribs play a chord by themselves: minor, detuned, terrible. It hangs in the air and won't stop ringing.

Fuzz holds on.

KAZOO: one note, low, sagging flat, and long.

The second candle goes out with a red flash, local to the candle (3 f max, reduced saturation), and a thread of smoke.

FAIL x2 SLAMs in. `SHK_STAMP` `s_a3_04_fail`

He holds on for one breath more. The bones never hug back.

The chord finally rings out. He lets go, one paw, then the other. He steps back down, one step, two, and sits on the steps, small. Bloom climbs up and sits on the step below him. She doesn't say anything.

> **FUZZ**  
> *(flat with grief, to his paws)*  
> Eleven years ago tonight, I hugged someone who needed it. <!-- a3_04_fuzz_02 -->

[R13]

> **FUZZ**  
> *(nothing left)*  
> It didn't work then either. <!-- a3_04_fuzz_03 -->

[HOLD 4 s: 120 f, breathing idles only] Fuzz and Bloom on the steps. Above them, the possessed twitches. One gold flame on the rail.

[NO BUTTON]

HARD CUT TO:

**Timing.** 14:10-14:50 · beat 40.0 s · **dialogue ≈ 10.6 s** (20 words; Fuzz at about 2 words a second on the steps).
* Layout: creak and "Father, don't." 0-1.6 s · the pat and "Nothing is unhuggable." 1.9-4 s · the climb 4-16 s · contact and the chord at 16 s, ringing to ≈ 22 s · kazoo 19 s · candle 20 s · FAIL x2 20.3 s · letting go and sitting 20.5-27 s · the confession 27-32 s · R13 · "It didn't work then either." 32.4-35.2 s · HOLD 4 s to 39.2 s, then the shot holds to the cut at 40.0 s.
* **The 12 s climb is the beat's slack.** If the animatic needs time anywhere in this beat, take it from the climb, never from the hold.
* Unscored ≈ 34 s: everything except the chord's ring and the kazoo, which are diegetic.
* No gag after the chord. Pip's paw over Mr. Flops' eyes comes before it, and is played straight.

### SC44 · 3-05 · INT. BAKE TENT - NIGHT (FIRST PERSON)
`14:50-15:20 · 0:30 (900 f) · first person · cap 0 · used 0 (plus "beep.")`

HARD CUT IN on the bar line. Frame 0: the insert song comes back, on the pre-chorus's last two bars again: "Knock, knock, knock! Come out!"

FIRST PERSON, through the back flap of the BAKE TENT, hellified. The HUD is back: two hearts, one shell, the plate reading SLAUGHTER.

[f0-120] The Big Puff has come down through the roof and filled the tent. It sits on Nana's stall like a hen on an egg, and poking out from under it is NANA'S RECIPE TIN. Its eyes roll down to the lens.

[~f130] It spits. SPLAT: a gob of demon blood across the lens, and a heart pops off the HUD (two to one). The red slides off on twos.

[~f150] [POP 4] On the HUD, the pet's face grows a horn nub. Its eyes go red. It glares.

[~f200]

> **PET**  
> *(from the HUD)*  
> beep. <!-- a3_05_pet_01 -->

CAPTION: [beep.]

[R5] The whole Big Puff flinches.

[~f260] BLAM: the last shell. [HIT-STOP 3] `SHK_SHOTGUN`. Half its face is gone: mist, teeth coming down like hail, an eye spinning off into the dark. A tooth *tinks* off the lens. The other half of the face is still snarling.

MUSIC: the chorus, sweetly: "Say it out loud! Out loud!"

CHK-CHK. The shell count reads 0.

[~f360] The Big Puff lunges, half a mouth open wide enough to take the lens. CLICK. Empty.

[R13] Silence. The song stops dead with the gun: a stop-time.

[~f380-400] The weapon slot SPINS like a fruit machine (*brrrrrt*) and lands on the WELDED SAW. Its plate flips: PURRING SAW → MEATCLEAVER. `s_a3_05_plate_saw` MUSIC: the song slams back in on its last line: "Doomtastic, Doomtastic—"

[~f420-450] A rev. It PURRS (the filter's sound), then it SCREAMS (the truth's), under the chorus's last hit: "—Times!"

[~f450-600] The saw goes in at the crown of the Big Puff's head and goes all the way down. `SHK_SAW`. A geyser of red bursts up through the tent's roof and comes back down as red rain.

[~f600] The boss pop: [HIT-STOP 8] plus 1 reduced white frame. The lens is painted red, ramping in over 8 f. It holds and drips, slowly.

[~f630-655] Through a gap in the drips, an eyeball rolls along Nana's stall and stops on the lid of the tin, facing the Wall. *Tik.*

[~f660-681] REWIND. The tape-rewind sound; hellify runs backwards over 21 f. The red drains up off the lens. The geyser runs back down into its hole and comes out again as a shower of hearts. The eyeball rolls backwards off the tin. The HUD drops out of frame.

[f681-900] The filter. MUSIC: a cute bed. Hearts float down through the hole in the roof onto everything. One tuft of plush stuffing drifts down and lands on the lid of the tin, where the eyeball was.

Sunny's paws come into frame and lift the tin. The view turns, the front flap brushes past the lens, and outside it turns back and looks up at the sign over the flap.

[ONE CHANGE: the sign now reads BAKE TEN.] `s_a3_05_tent_sign` [HOLD for reading: at least 33 f]

**Timing.** 14:50-15:20 · beat 30.0 s (900 f) · **dialogue ≈ 0.2 s** ("beep.").
* The frame plan is the beat sheet's: tin 0-120 · nub ~150 · beep ~200 · flinch R5 later · last shell ~260 (hit-stop 3) · CLICK ~360 and R13 · slot spin and plate flip ~380-400 · rev ~420-450 · saw ~450-600 · boss pop ~600 (hit-stop 8 and 1 reduced white frame) · red lens and the eyeball ~600-660 · REWIND ~660-681 · the filter, the stuffing and BAKE TEN to 900.
* **Added:** the spit at ~f130, so the horn nub at ~f150 follows the bible's rule that the nub shows at one heart. Sunny leaves 3-03 on two hearts.
* **Music, suggested conform:** f0 the pre-chorus's last 2 bars · f96 the chorus · "Say it out loud! Out loud!" at ≈ f282-330, over the half face · the CLICK at f360 is a one-beat stop-time · back in at f384 on "Doomtastic, Doomtastic" under the spin · the last "Times!" at f432, under the rev. In 3-03 the song was cut off on "Say it out—"; here it gets to finish the line.
* **Harding:** the red lens ramps in over 8 f and drains over 21 f (in the REWIND), so it is never a strobe. Log it.
* The REWIND clears the eyeball before the tin reaches Pip in 3-06. No sticker in this scene. No child in the tent.

### SC45 · 3-06 · EXT. BANDSTAND STEPS - NIGHT
`15:20-15:55 · 0:35 · cap 8 · used 0 (silent by design)`

Frame 0: *pff*. A heart lands on the lid of the tin.

THE BANDSTAND STEPS. Hearts are still drifting down out of the dark. Up through them from the fair comes Sunny, carrying NANA'S RECIPE TIN in both arms, with the tuft of stuffing on its lid and the pet on their head. MUSIC: the cute bed from 3-05 plays on.

The crowd comes out from behind the hay bales [!RIPPLE]. Under the cake table the tablecloth lifts, and the Mayor crawls out, top hat first, and stands up brushing at his knees, as if he'd only dropped something.

Fuzz is still on the steps where we left him, Bloom on the step below. Up on the bandstand, possessed Bonejangles twitches. One gold flame on the rail.

Sunny holds the tin out to NANA. [R8] She takes it in both paws, the way you'd take back a baby you'd lent somebody, and sits on the step beside Fuzz with it on her knees. Down on the grass in front of them, Pip gets up on tiptoe to see, and holds Mr. Flops up so he can see too. The Mayor leans in over Nana's shoulder.

The lid comes off. *Tunk.*

Inside are Nana's Mingle keepsakes: a dried lantern-flower, a curl of ribbon, a cookie cutter shaped like a little horn, and a slip of paper in a round, careful hand: *Charlotte's turn: #9*. `s_a3_06_slip` She lifts them out one at a time and sets each one down gently.

Under them, folded in four, is a sheet of paper. She opens it out.

*MIDSUMMER MINGLE · WHO'S BRINGING WHAT?* `s_a3_06_sheet_title`

MUSIC: a fragment of the NANA motif, 2 bars.

The camera tilts down the column, one name every 26 f, each landing with a small pop. Names from both sides of the Wall share the one column, in a dozen different hands. `s_a3_06_sheet_01` to `_06`

*Nox: bone soup (2 bowls)*

*Giggs: cupcakes (+1)*. A plump little hand (the Mayor's) darts into frame and folds the corner down over the "(+1)". [4 f] *Fwip.* The hand withdraws.

*Charlotte: cookies (sorry)*. Nana's thumb comes to rest on it. [R21]

*Fuzz: hugs*. Fuzz's ears go down a pixel.

*Bonejangles: music*. Up on the bandstand, with nobody touching them, the possessed ribs give one soft note. *Plink.*

MUSIC: out. [SILENCE]

*Clinker: a poem (will rate yours)* [HOLD 13]

The corner of Nana's mouth twitches. Fond.

[BUTTON +13: up on the bandstand, the possessed skull turns one pixel toward the sheet, with a creak.]

The looks, R8 apart:
* Fuzz looks from the sheet to the boards, where the chalk still reads *...where the pumps go clink, / we work all night, and we-*;
* [R8] then up at the bones;
* [R8] Nana looks at Fuzz;
* [R8] the Mayor looks away, at nothing in particular.

Down on the grass, Pip's ears go straight up. Pip has read it too.

MUSIC: the FUZZ hymn motif, small.

Fuzz gets to his feet, the first time he has stood since the hug. He takes the sheet from Nana carefully, as if it might tear, and looks at the last candle. Then he climbs back up the steps, and this time he doesn't stop until he's at the top.

**Timing.** 15:20-15:55 · beat 35.0 s · **dialogue 0 s**. The breakdown recommends none, and the sheet is read by the camera, never aloud.
* Layout: the arrival and the crowd 0-6 s · the hand-over 6-9 s · the lid, the keepsakes and the slip 9-12 s · the sheet's title 12-14 s · the names 14-21.9 s (26 f each, plus the fold's 4 f, the thumb's R21, and HOLD 13 and reading time on the last) · the twitch and the button 21.9-22.8 s · the looks and Pip 22.8-26 s · the hymn motif and Fuzz's climb 27-35 s.
* The NANA motif runs from 14 s and stops as "Bonejangles: music" lands. Unscored ≈ 7.5 s (19.6-27 s), against the 8 s planned for the last name.
* The laughs are the "(+1)" fold and "(will rate yours)". The thumb on "Charlotte" is not a joke: hold it.

### SC46 · 3-07 · EXT. BANDSTAND - NIGHT
`15:55-16:40 · 0:45 · ACT OUT · cap 22 · used 18 (+ 12 sung)`

TRY 3.

The top of the bandstand steps.
* FATHER FUZZ squares up to the possessed bandleader again, the sign-up sheet tucked in his satchel strap. One gold flame on the rail.
* Bloom is a step below him.
* Sunny stands at his side, the pet on their head.
* Pip is behind him, down at the foot of the steps with Nana, Mr. Flops under one arm.
* The Mayor sits off to one side of the steps, his clipboard flat on his knees.

> **FUZZ**  
> *(renewed; a coach before the match)*  
> All three. From the top. <!-- a3_07_fuzz_01 -->

THE RHYTHM. On the next downbeat the pet starts to beep, on every beat of the 140 bpm grid, steady as a clock.

> **PET**  
> *(metronome: one per beat)*  
> beep. <!-- a3_07_pet_01 -->

CAPTION: [beep. beep. beep. beep.]

Fuzz claps on two and four and sings the St. Cuddles hymn, bright and in major: the phrase that lost the duel in 2-06.

> **FUZZ**  
> *(SUNG, in major; holds the tempo)*  
> Come in close, come in close,  
> hold the one who's frightened most. <!-- a3_07_fuzz_03 -->

The ribs answer him in minor, as before, a hair behind, pulling. This time something holds him.

* **Bar 1:** "Come in close," on the grid, on the beep. The ribs echo it in minor.
* **Bar 2:** "come in close," and the ribs drag, and Fuzz's claps start to drift... Behind him, Pip claps on two and four, with the beep. Then Nana. Then the whole crowd claps in [!RIPPLE] `w_giggle_09`: the crowd that wouldn't join in at 2-06. His claps come back onto the grid.
* **Bar 3:** "hold the one who's." The ribs pull harder. Nothing moves. Bloom lifts two fingers, as if taking a pulse from the air.

> **BLOOM**  
> *(clinical; almost surprised)*  
> ...Steady. <!-- a3_07_bloom_01 -->

* **Bar 4:** "frightened most," on the grid, in major. The ribs' echo trails off, out of things to say. The last flame stands up straight and tall.

Fuzz hums on. The beeps and the claps keep time under everything that follows.

THE WATER. Fuzz takes a small flask from his satchel and puts it in Sunny's paws.

> **FUZZ**  
> *(soft, to Sunny)*  
> Gently. <!-- a3_07_fuzz_04 -->

Sunny cups one paw, fills it, and pours it over the ribs [21 f], gently, the way you'd water a seedling. A soft hiss. A curl of steam. Where the water lands, the purple dims one shade. The bones shudder, and the ribs miss a note. The flame grows.

THE NAME. Fuzz drops to one knee beside the last candle, unfolds the sign-up sheet and holds it up to its light. Over his shoulder, his finger runs down the column (*Nox... the folded corner... Charlotte... Fuzz... Bonejangles...*) and stops on the last line. He breathes in. [8 f]

> **FUZZ**  
> *(reading it out; it catches)*  
> Cl- <!-- a3_07_fuzz_02 -->

[FREEZE: cut off on frame 0] [MUTE] Everything stops on the same frame: the hymn, the claps, the beep. Fuzz's mouth stays open on the shape of the next sound. Nothing comes out of it.

[R21]

On the steps, the Mayor doesn't look up from his clipboard.

> **MAYOR**  
> *(quietly, from the steps; almost kind)*  
> Father. <!-- a3_07_mayor_01 -->

[R13]

> **MAYOR**  
> *(same; every word equal weight)*  
> Nobody on this side says those names. <!-- a3_07_mayor_02 -->

[FREEZE ALL ADULTS] [!RIPPLE 0-3 f] One idle clock after another stops: the crowd, Nana, Bloom, and last of all the Mayor, mid-breath. Fuzz is already stopped. Of the grown-ups, only the possessed bones keep twitching.

Three others still have a running clock:
* Pip, whose ears turn from one grown-up's face to the next;
* the pet, looking at Sunny;
* and Sunny, who could stand perfectly still if they wanted to, and for once doesn't. One ear turns toward the sheet.

[HOLD 2 s]

The last flame shrinks to a blue bead. [26 f] MUSIC: the WALL motif (E-F-E-B♭), low, once.

[SILENCE] The town stays frozen. The bones twitch. The bead burns. [R26]

> **POSSESSED BONEJANGLES / CLINKER**  
> *(both voices; dead flat)*  
> {d}We'll wait. <!-- a3_07_bone_01 + a3_07_clink_01 (sync late=R8) -->

The words just appear. The blue bead holds.

> **ACT OUT (16:40).** Rhythm and water have both landed, and the name is written right there. The Mayor: "Father. Nobody on this side says those names." Every adult is frozen. The last flame is a blue bead. {d}"We'll wait." Cut on the flame's gutter, the frame-0 sound of the OUT card.
>
> *The question the break leaves:* Who on this side will say it?

**Timing.** 15:55-16:40 · beat 45.0 s · **dialogue ≈ 8.7 s** (18 words) **plus the sung phrase ≈ 6.9 s** (4 bars at 140 bpm).
* Layout: "All three. From the top." 0.5-2.4 s · the rhythm 2.6-9.5 s ("...Steady." in bar 3, ≈ 7.4 s) · the water 9.5-15.5 s · the name 15.5-24.5 s · "Cl-" 24.5 s · R21 · "Father." 25.4-26 s · R13 · "Nobody on this side says those names." 26.4-29.9 s · the freeze ripple 29.9-31.5 s · the three who still move 31.5-33.5 s · the flame 33.5-34.4 s, with the WALL motif to ≈ 38 s · silence 38-41.3 s · "We'll wait." 41.3-42.5 s · the bead holds to the cut at 45.0 s.
* Unscored ≈ 16.5 s ("Cl-" to the WALL motif, then the motif's end to the cut), against the 15 s planned.
* No gag. The name gets an R13 or more on either side of it and no button, ever (`GAG_BIBLE.md` §6.22). The Mayor's reaction is its B-side, at the first step: he stops it.

---

**END OF ACT THREE**

---

## EYECATCH PAIR 3 (BREAK 3)
`SC47-SC48 · 16:40-16:50 · 0:10 · no dialogue`

### SC47 · EC3-OUT · OUT CARD: THREE CANDLES (CLOCK 22:59)
`16:40-16:45 · 0:05`

SFX: on frame 0, the flame's gutter (the act out's sound). Then the HEE_HEE bell.

Three candles on the bandstand rail, drawn the way HaHaland would draw them. Two are out, with little curls of smoke like question marks. The third burns a cheerful gold. `s_ec3out` In the corner the town clock reads 22:59. Pure HaHaland. A seamless 5 s loop.

MUSIC: the theme's hook in major, 2 bars.

### SC48 · EC3-IN · IN CARD: THREE CANDLES, ONE CHANGE
`16:45-16:50 · 0:05`

[FLASH: hellify, 4 f, reduced luminance, Harding-checked]

The same card with exactly one change: the last flame leans toward the Wall (screen-right), as if it's listening. `s_ec3in`

MASCOTTY stands in the deep background, 8 px tall, at the same depth as in EC1-IN and EC2-IN.

MUSIC: the RIFF, resolving on the ITS_FINE sting.

---

## ACT FOUR, "23:00 ________"
`16:50-20:45 · 3:55 · 8 beats · SC49-SC56 · cap 117 · used 105`

**Act shape.**
* Nobody on this side can say the name. Sunny opens their mouth, and nothing comes.
* A child says it. The breath goes home and, for the first time, arrives on time.
* A hug for the priest. Then the Mayor's coldest line.
* One small firework, and an answer.

Protected holds 2 (4-03) and 3 (4-08) are in this act. The last story beat has no button: its button comes after the credits.

**Continuity in, from writer 3's 3-07.** This draft opens on 3-07's last frame as the beat sheet stages it. If writer 3's staging differs, match theirs.
* FATHER FUZZ is on one knee in front of the possessed BONEJANGLES. He holds the sign-up sheet up, turned toward himself to read it. His mouth is still on "Cl-". DR. BLOOM is a step below him (writer 3's 3-07 staging).
* PIP stands behind Fuzz, down at the foot of the steps beside Nana (writer 3's 3-07 staging), holding MR. FLOPS.
* MAYOR GIGGS sits off to one side of the bandstand steps, his clipboard on his knees. NANA PUDDIN is at the foot of the steps.
* SUNNY is at the rail with the pet on their head, one paw still wet from the holy water.
* Three candles stand on the rail. Two are out; the third is a blue bead.
* Every adult in the town is frozen. Only the bones still twitch. Three clocks still run: Pip's, the pet's and Sunny's (writer 3's 3-07).

### SC49 · 4-01 · ACT CARD / EXT. BANDSTAND - NIGHT
`16:50-17:10 · 0:20 · cap 1 · used 1`

CARD: THE MAYOR'S CLIPBOARD, in the CREAM window skin. The whole night is on it: *18:00 Welcome!* ✓ · *19:00 Bake-Off!* ✓ · *20:00 Bandstand!* ✓ · *21:00 (we don't do that)*, underlined twice · *22:00 ~~Bedtime!~~ weather*. Then the next line pops in [8 f]: `s_a4_01_card`

*23:00 ________*

It's blank. He never planned past bedtime.

Faintly, back to front, a line of somebody else's handwriting shows through the paper from a sheet clipped behind. It is too faint to read.

SFX: the HEE_HEE bell, once, flat: one dull ding, a semitone low, with no jingle after it.

CUT TO:

THE BANDSTAND. The freeze holds exactly where Act Three left it. Nobody breathes. Nobody blinks.

Only the possessed Bonejangles moves: red sockets, a purple ribcage, his ears and ribs twitching on every frame. On the rail, the blue bead burns very small.

Nana's mouth works. It finds the first sound.

> **NANA**  
> *(the sound dies in her mouth)*  
> Cl... <!-- a4_01_nana_01 -->

[R13] Her eyes go to the Mayor.

[R13] The Mayor looks at the sky, and keeps looking at it.

[R13] Sunny opens their mouth [8 f].

Nothing comes. Not a word, not a breath.

[R13] The pet leans over the edge of Sunny's head and looks down into Sunny's face, upside down.

[R13] Sunny closes their mouth [13 f].

[BUTTON +13: on the rail, the blue bead leans toward the Wall, the way it did on the IN card.]

[SILENCE] holds to the cut.

**Timing.** 16:50-17:10 · beat 20.0 s · **dialogue ≈ 0.4 s** (1 word; the breakdown's talk window is 0.5 s). Layout: the card 0-3.0 s (the bell on frame 0, the blank line pops at 0.3 s) · the held freeze 3.0-9.0 s · "Cl..." at about 9.0 s · Nana's look and the Mayor's, R13 apart, 9.4-11.3 s · Sunny's mouth opens at 11.7 s, nothing comes for about 1.2 s, the pet looks at 13.3 s, and the mouth closes 13.7-14.1 s · the button at about 14.5 s · silence to the cut at 20.0 s. The whole beat is unscored (the bell is a sound effect). There is no gag here: Sunny's silence is made into a choice. **Writer's note:** the countdown to Sunny's three canon lines starts here, with a closed mouth. Nothing on screen says so.

### SC50 · 4-02 · EXT. BANDSTAND - CONTINUOUS
`17:10-17:40 · 0:30 · cap 23 · used 21`

SFX on frame 0: a SNIFF.

PIP, behind Fuzz. Of the town's own, only Pip is still breathing, blinking and twitching an ear: one idle clock still running among a hundred stopped ones (the newcomers, Sunny and the pet, are the only others). **FC:** Pip's, Sunny's and the pet's clocks run; every other adult's reads 0. Nobody has taught Pip to freeze yet.

Pip is crying, and has Mr. Flops in a fierce two-armed headlock, the way you hold something so it doesn't have to see.

Pip looks from face to face. Nana, frozen. The Mayor, frozen, still looking at the sky. Fuzz, frozen in the middle of "Cl-".

MUSIC: a low OMINOUS bed creeps back in under the twitching ribs.

> **PIP**  
> *(crying; holding it together, officially)*  
> My eyes are just doing a rain. <!-- a4_02_pip_01 -->

[R13] Nobody moves. Nothing is funny.

> **PIP**  
> *(cracking on NAME)*  
> ...It's just a NAME. <!-- a4_02_pip_02 -->

Pip climbs the steps behind Fuzz, past frozen Bloom, and stops one step below him: Fuzz is still between Pip and the bones. Up on tiptoe, Pip reads the sheet over his shoulder. Finds the name.

[MUTE ALL] Everything goes: the bed, the twitching, the night air.

[R5]

> **PIP**  
> *(full chest; dry and alone)*  
> CLINKER! The name is CLINKER! It says so RIGHT HERE! <!-- a4_02_pip_03 -->

Pip's paw jabs over Fuzz's shoulder at the name on the sheet. **FC:** the voice plays dry and unprocessed, the only sound in the mix.

[R13] The silence holds after the name.

MUSIC: the TRUE NAME motif, soft.

The purple drains out of the ribs [21 f], from the top rib to the bottom. The red goes out of the sockets. The twitching stops.

On the rail, the blue bead swells back into a warm gold flame.

MUSIC: the CHOIR comes in under it, soft, not triumphant.

The grown-ups thaw in a ripple [!RIPPLE]: breath, blink, ears. The Mayor thaws last, R8 behind everyone, and his wings restart with a stutter.

Fuzz lowers the sheet, slowly, and turns on his knee to look at the small person behind him.

Pip is still crying. Pip lets go of the headlock and holds Mr. Flops properly.

[NO BUTTON] The name never gets one (`GAG_BIBLE.md` §6.22).

**Timing.** 17:10-17:40 · beat 30.0 s · **dialogue ≈ 7.4 s** (21 words: 2.6 s and 1.5 s crying, then 3.3 s shouted), plus 0.9 s of gaps; the breakdown's talk window is 10 s. Layout: the sniff and Pip's look round 0-5.0 s (the bed comes in at 1.5 s) · "rain" 5.0-7.6 s · R13 · "NAME" 8.0-9.5 s · the climb and the tiptoe 9.5-11.0 s · MUTE ALL at 11.0 s, then R5 · "CLINKER!..." 11.2-14.5 s · R13 · the TRUE NAME motif, the 21-f drain and the flame 15.1-17.5 s · the CHOIR, the thaw and Fuzz's turn 17.5-25.0 s · Pip and Mr. Flops 25.0-30.0 s. Unscored ≈ 5.5 s (the first 1.5 s, and from MUTE ALL to the motif). **The R13 before the name** (§6.22): the climb and the tiptoe give about 1.5 s with no speech before the name, and MUTE ALL then R5 stays exactly as the beat sheet times it (the beat sheet wins every conflict, so MUTE ALL is not moved earlier). The R13 after the name is written in. **Safety:** Pip stays behind Fuzz for the whole beat, and neither the fog nor the breath ever comes near them.

### SC51 · 4-03 · EXT. THE SEALED GATE - NIGHT
`17:40-18:25 · 0:45 · PROTECTED HOLD 2 of 3 · cap 36 · used 33`

Bonejangles' ribcage lets out a breath: a long, soft sigh. The ribs settle [10 f]. No FWOOMP.

Out between two ribs comes the breath: a small, tired puff, no bigger than a muffin, grey-lilac, with no face. It sags like somebody at the end of a long shift, and it trails a thin line of chalk dust.

MUSIC: the GOODBYE motif, under the puff.

It drifts south at a constant speed, away from the steps where Pip stands. It passes over the torn-up boards and over its own unfinished poem in chalk, *...and we-*, without stopping.

It reaches the gate in the Wall beside the bandstand. SIGN: seven padlocks, KEEP OUT. FOREVER. `s_gate_keepout` Under them, the small barred grille, its shutter shut. The puff stops at the keyhole.

[R13] It waits there, the way you wait when you've lost your key.

At the bandstand, Bonejangles comes back into himself. Two warm pinpricks light in the dark sockets. His jaw drops open and re-seats with a *click*. He sits down hard on the top step, with a soft clatter.

> **BONEJANGLES**  
> *(himself again; small, confused)*  
> ...Clack? <!-- a4_03_bone_01 -->

[R13]

> **BONEJANGLES**  
> *(gentle, puzzled)*  
> Something was in there with me. It kept trying to finish a poem. <!-- a4_03_bone_02 -->

He looks across at the puff by the keyhole. It is still waiting.

He finds his mallets on the step and counts himself in with two soft clacks on his collarbone. The GOODBYE motif hands over to him and goes.

He plays the hook on his ribs: HaHaland Day in C major at 140 bpm, the redeemed form. **FC:** the fourth note lands on C, not B♭.

> **BONEJANGLES**  
> *(spoken-sung, in time; warm)*  
> Down by the Wall where the pumps go clink, / we work all night, and we dream in pink. <!-- a4_03_bone_03 -->

The last note rings out.

The puff waits until the note has quite finished ringing. Then it goes through the keyhole, a little at a time, with a sound like somebody blowing softly across the top of a bottle. A smudge of chalk dust is left on the lip of the keyhole.

[R21] [SILENCE]

From the other side of the Wall comes a voice, small, and for the first time on time:

> **CLINKER** (V.O.)  
> *(beyond the Wall; small; on time)*  
> ...Seven. <!-- a4_03_clink_01 -->

**FC:** `late=0`. There is no 8-frame slip, because the voice was always on the other side.

[HOLD 4 s: 120 f] The gate. Bonejangles on the step, the mallets in his lap. Behind him the town, thawed and breathing. Idle clocks only. No music, no gag.

[NO BUTTON]

**Timing.** 17:40-18:25 · beat 45.0 s · **dialogue ≈ 14.5 s** (33 words: "...Clack?" 0.6 s, his line 6.2 s at 2.1 words a second, the couplet 6.9 s, "...Seven." 0.8 s); the breakdown's talk window is 17 s. Layout: the sigh 0-2.0 s · the drift 2.0-13.5 s at constant speed · R13 at the keyhole · the wake 13.9-17.0 s · "...Clack?" · R13 · the line 18.0-24.2 s · the look and the count-in 24.2-27.0 s · one bar of the hook 27.0-28.7 s · the couplet over 4 bars 28.7-35.6 s · the ring and the keyhole 35.6-39.5 s · R21 · "...Seven." 40.2-41.0 s · HOLD 4 s, 41.0-45.0 s. Unscored ≈ 8 s (from the end of the ring to the cut). **Protected:** never trimmed and never undercut. On screen the breath is only "something" and "it", never "she", which keeps "That was a PERSON." for 107.

### SC52 · 4-04 · EXT. BANDSTAND STEPS - NIGHT
`18:25-18:45 · 0:20 · cap 8 · used 7`

Fuzz sits on the steps, on the same step as in Act Three. The last candle is in his paws, its flame gold again. He looks down at his paws. They are shaking a little, the way they have since nine o'clock.

> **FUZZ**  
> *(small, to his paws)*  
> ...I got lucky. <!-- a4_04_fuzz_01 -->

[R13]

> **FUZZ**  
> *(rueful; the smallest smile)*  
> Luck isn't a liturgy. <!-- a4_04_fuzz_02 -->

Sunny looks at him. Then hugs him anyway [SQUASH 6 f]. There's no sound but plush: *pff*.

MUSIC: the FUZZ hymn's amen, solo, 2 bars at half tempo.

Fuzz goes stiff with surprise. [R13] His free arm comes round Sunny.

[BUTTON +13: his other arm, the one with the lit candle, goes straight out to the side, holding the flame well clear of two people made of plush.]

**Timing.** 18:25-18:45 · beat 20.0 s · **dialogue ≈ 4.0 s** (7 words at about 1.8 words a second), plus R13; the breakdown's talk window is 4 s. Layout: the sit and the paws 0-3.0 s · "...I got lucky." 3.0-4.8 s · R13 · "Luck isn't a liturgy." 5.2-7.4 s · Sunny's look and step 7.4-10.0 s · the hug lands at 10.0 s · R13 · the button at about 10.9 s · the amen 10.2-17.1 s · the amen rings out to the cut at 20.0 s. Unscored ≈ 10 s (before the amen). The hug answers "Nothing is unhuggable." The button keeps the beat light, as the beat sheet asks: his crisis is seeded here, not spent.

### SC53 · 4-05 · EXT. BANDSTAND - NIGHT
`18:45-19:10 · 0:25 · cap 22 · used 22`

The Mayor flaps up onto the bandstand rail, wings buzzing again, clipboard under one arm. Every inch the host. MUSIC: the MAYOR motif, too bright.

> **MAYOR**  
> *(brightly, arms wide, to everyone)*  
> Right! Everybody home! Lovely weather tonight! Lovely, lovely weather! <!-- a4_05_mayor_01 -->

> **CROWD** (WALLA)  
> *(a relieved ripple)*  
> (relieved sighs, a laugh) <!-- w_giggle_11 -->

The Giggle lets its breath out [!RIPPLE]. Hats go back on.

> **MAYOR**  
> *(cheerful, wagging a finger)*  
> Nobody look at the boards! <!-- a4_05_mayor_03 -->

Every head turns to look at the boards [!RIPPLE]. Every head snaps back [!RIPPLE, at 2x].

> **MAYOR**  
> *(pen out, with a flourish)*  
> Let's make it OFFICIAL! <!-- a4_05_mayor_04 -->

He lifts the schedule to write on the blank line, and the sheet clipped behind it flips up. It is the pale grey fourth sheet from 20:00. It isn't his paper, and it isn't his handwriting.

SIGN: *You never asked where it went.* `s_a4_05_draft`

[FREEZE: the Mayor; his wings stop. HOLD 69 f, the sheet's reading time.] Under it, the MAYOR motif plays on, as bright as ever.

He folds it very fast [4 f]: fold, fold, and into his waistcoat.

[MUTE 14]

> **MAYOR**  
> *(brisk, far too casual)*  
> Old draft. <!-- a4_05_mayor_02 -->

MUSIC: the MAYOR motif re-enters on the grid, exactly as bright as before.

At the foot of the steps, Nana looks at him for exactly R8. She says nothing.

[BUTTON +13: on the blank line, in his round, happy hand, the Mayor writes *23:00 Home!* Then, from the same waistcoat pocket the folded page went into, he takes the Bake-Off stamp and stamps it: *A+*. `s_a4_05_home` A grade it hasn't earned.]

The Giggle goes home in twos and threes. As the crowd thins, Pip climbs up onto the MINGLE crate and sits down.

**Timing.** 18:45-19:10 · beat 25.0 s · **dialogue ≈ 7.1 s** (20 words at about 2.8 words a second), plus the sheet's 2.3 s reading hold (0.3 s a word plus 0.5 s, `GAG_BIBLE.md` §4.13); the breakdown's talk window is 8 s. Layout: up onto the rail 0-1.5 s · the locked line 1.5-4.9 s · the relief ripple 4.9-6.4 s · "boards" 6.4-8.1 s · the head-turn ripple 8.1-9.6 s · "OFFICIAL" 9.6-11.0 s · the lift and the flip 11.0-12.4 s · FREEZE and reading hold 12.4-14.7 s · the 4-f fold · MUTE 14 · "Old draft." 15.3-16.1 s · Nana's R8 · the button 16.8-19.0 s · the crowd goes home, and Pip climbs onto the crate, 19.0-25.0 s. Unscored ≈ 0.5 s (the MUTE 14). **The new words** all sit in the bright patter before the flip, as the breakdown asks, and nothing new comes after it. "Lovely, lovely weather!" is the "weather side" callback. **The button's A+** is the Bake-Off stamp from 1-09 ("Mm! A-plus!"): Hillvile grades with letters, a minute after Hellvile counted "...Seven." (`GAG_BIBLE.md` §6.19), and "Let's make it OFFICIAL!" sets it up.

### SC54 · 4-06 · EXT. BANDSTAND, THE MINGLE CRATE - NIGHT
`19:10-19:45 · 0:35 · cap 26 · used 22`

The bandstand, emptied. The south boards still lie where the gust threw them. In the gap sits the crate, stencilled MINGLE · DO NOT OPEN, with a little stencilled rocket under the words `s_a2_03_crate`. PIP sits on it, legs swinging, Mr. Flops in their lap. Nana, Fuzz with the candle, Bonejangles, and Sunny with the pet are still here. The Mayor comes down the steps.

MUSIC: the PIP motif, pizzicato, slower than it was at 19:40.

> **PIP**  
> *(proud, tired)*  
> I didn't cry. <!-- a4_06_pip_01 -->

[R8]

> **PIP**  
> *(honest, smaller)*  
> I did a little. <!-- a4_06_pip_02 -->

[R13]

> **PIP**  
> *(very small; hopeful anyway)*  
> ...Not even one firework? <!-- a4_06_pip_03 -->

MUSIC OUT.

The Mayor looks at the crate. [R13] Then at the Wall. [R13] Then at the last candle in Fuzz's paws.

[HOLD 21]

> **MAYOR**  
> *(quiet; deciding)*  
> One. <!-- a4_06_mayor_01 -->

[R13]

> **MAYOR**  
> *(a little warmer)*  
> A small one. <!-- a4_06_mayor_02 -->

[R13]

> **MAYOR**  
> *(to Pip, gently)*  
> For the patrol. <!-- a4_06_mayor_03 -->

Pip's ears go straight up. Pip hops down off the crate.

MUSIC: the MAYOR motif, slow.

> **PIP**  
> *(instantly official)*  
> Patrol: three steps back. <!-- a4_06_pip_04 -->

[3x@8] Step. Step. Step. Pip stops and looks down at Mr. Flops. [R13] One more step, for Mr. Flops.

[BUTTON +13: without a word, Nana takes one small step back too.]

The Mayor stands over the crate. DO NOT OPEN. He opens it. The lid creaks for the first time in eleven years. Dust. Inside, packed in straw, is a nest of old rockets, each stencilled like the lid. He lifts out the smallest and blows the dust off. Underneath, the paper is pink.

Fuzz holds out the last candle. The Mayor takes it in his other paw, carefully, the way you take something lit.

He turns to the old path up the hill. The others fall in behind him, with Pip four steps back.

**Timing.** 19:10-19:45 · beat 35.0 s · **dialogue ≈ 7.7 s** (22 words: Pip at the 1.3x cast tempo, the Mayor slow), plus 2.3 s of written gaps (R8, R13, HOLD 21, R13, R13); the breakdown's talk window is 10 s. Layout: the empty bandstand and the crate 0-3.0 s · Pip's three lines 3.0-7.4 s · the looks, R13 apart, 7.4-11.0 s · HOLD 21 · "One." / "A small one." / "For the patrol." 11.7-15.0 s · the hop down 15.0-16.0 s · "Patrol..." 16.0-17.5 s · the steps, 8 f apart, then the fourth R13 later, 17.5-19.0 s · the button at about 19.4 s · the crate 19.4-27.0 s · the candle 27.0-32.0 s · the turn to the path 32.0-35.0 s. Unscored ≈ 7.6 s, from Pip's last question to "For the patrol." **The gag** is the safety staging: the kid ends up well back from the grown-up with the firework, and the extra step is the topper on a rule of three. **The callback:** in 1-10 Pip told Sunny "I'm the whole patrol." [R8] "And Mr. Flops!", so "For the patrol." is a firework for one kid and one toy, and the extra step is Mr. Flops' own.

### SC55 · 4-07 · EXT. THE JOKE GRAVEYARD, LAUNCH HILL PATH - NIGHT
`19:45-19:57 · 0:12 · cap 0 · used 0`

Side-on, at a constant walking pace, the procession crosses the JOKE GRAVEYARD from left to right:
* the Mayor first, the rocket under one arm and the candle cupped in the other paw;
* Sunny right behind him, with the pet on their head;
* Nana, holding Pip's paw, with Mr. Flops;
* Fuzz;
* Bonejangles at the back, clicking softly as he walks.

MUSIC: the slow MAYOR motif carries on from the crate.

Joke graves slide past as texture. SIGN: *Rest in pieces, Mr. Huggins the 1st* `s_a4_07_grave_huggins`. Others pass behind it, not held for reading: THE GREAT CAKE OF '89 `s_a4_07_grave_cake`, GRANDPA PUFF'S MOUSTACHE `s_a4_07_grave_moustache`.

Then a small stone, child-sized, lettered in a child's hand. SIGN: FLUFFY THE CLOUD. DRIFTED. `s_a4_07_grave_fluffy` There is no year. On top of it sits a cupcake with pink icing, and it is fresh.

[MUTE 13] On frame 0 of it, the Mayor's wings stop. His feet don't.

His face goes sad for exactly R8. **FC:** 8 f of EXPR SAD, then EXPR HAPPY (canon W16).

The wings start again. The motif re-enters on the grid. He goes on.

Only Sunny, one step behind him, saw the wings stop. Sunny looks at the stone as they pass it, then at the back of the Mayor's hat, and keeps walking.

**Board note:** the camera stays low on the path, and the sky stays out of frame. No shot in this beat links the stone to the sky.

[BUTTON: the release is the wings starting again, and nothing else.]

**Timing.** 19:45-19:57 · beat 12.0 s · **no dialogue**. Layout: the procession and the joke graves 0-4.0 s (Mr. Huggins' stone in frame for at least 2.0 s) · the small stone enters at 4.0 s · the wings stop and MUTE 13 at 5.0 s, with the sad face for R8 inside it · the wings restart at 5.4 s · Sunny passes the stone 5.5-9.0 s · on up the hill 9.0-12.0 s. Unscored ≈ 0.4 s. **NO BUTTON ration:** the wings restarting is this beat's release, so 4-07 and 4-08 are not two un-buttoned beats in a row (`GAG_BIBLE.md` §4.17).

### SC56 · 4-08 · EXT. LAUNCH HILL - NIGHT
`19:57-20:45 · 0:48 · PROTECTED HOLD 3 of 3 · ACT END · cap 1 · used 1`

The top of the hill. An old iron launch stand, rusted, points south over the Wall. Below lies the whole park, dark, with the Wall straight down the middle of it, on the frame's centre line.

The Mayor sets the rocket in the stand. Pip stops well back, four steps behind the furthest grown-up, holding Mr. Flops tight. Sunny with the pet, Nana, Fuzz and Bonejangles stand in a loose line. At the very back, where nobody saw her arrive, stands MASCOTTY, perfectly still.

He touches the candle to the fuse. It catches. *Fsss.*

[MUTE] The score goes. The sequencer keeps time.

The rocket climbs [26 f], thin and quiet, and opens high over the Wall as a PINK HEART. *Thump*, soft, on frame 0 of the bloom, with no bang. The light ramps up over 8 f or more.

> **PIP**  
> *(barely voiced)*  
> ...Oh. <!-- a4_08_pip_01 -->

THE FACES. One wide, from below. Every face goes pink in a ripple [!RIPPLE]: Nana, Fuzz, Bonejangles, Sunny and the pet. And at the back, Mascotty's big bunny head, lit pink too.

THE MAYOR, close. He watches it. At the edges of his shot, the pixels bleed into crayon [8 f]: paper grain and a boiling line. For a moment the frame is the storybook again, and he is nine. His face stays his own (no crayon face for young Giggs, `GAG_BIBLE.md` §8.7).

[R26]

BRICK WIPE: bricks stack down the Wall line, 5 courses of 2 f each [10 f]. Then the wipe stops halfway.

THE SPLIT [60 f]. Left half: the hill, and the heart fading over the Wall. Right half: HELLVILE, shot tight. A sooty hand strikes a match, *scratch*, and lights a fuse, *fsss*. Beside it stands a table with two bowls on it. No faces. Each half keeps its own sound.

The wipe closes [10 f], right to left, with a trowel *tink* on the last course.

The pink heart sinks away in sparks. Then nothing.

Then, from the south, from over the Wall, a small PURPLE FIREWORK goes up and opens, small. *Thump.* It's an answer.

Nobody speaks. Every head turns to it [!RIPPLE]. Pink sparks and purple sparks fall side by side over the Wall.

The Mayor hums.

> **MAYOR**  
> *(hums the Mingle; one note further)*  
> Mm-mm, mm-mm-mm, mm-mm-mm, mm-mm-- <!-- a4_08_mayor_01 -->

**FC:** it is the same tune as at dawn (F major), and he gets exactly one note further than he did on the ladder. Then he stops.

[HOLD 4 s: 120 f] The last pink speck and the last purple speck over the Wall. Breathing idles only. No music, no gag.

[IRIS♥ out, 5 f] The heart iris closes on the two specks, with no *shwip*: the act's last beat takes no button, not even a sound one.

[NO BUTTON]

FADE TO BLACK.

> **ACT END (20:45).** There's no act-out line and no button. The last image is the pink heart fading over the Wall and a purple spark answering it. The tag starts after the black and a time cut.

**Timing.** 19:57-20:45 · beat 48.0 s · **dialogue ≈ 0.5 s** ("...Oh.") **plus the 4.6 s hum**; the breakdown's talk window is 0.5 s. Layout: the arrival and the stand 0-7.0 s (the slow MAYOR motif) · the lighting and the fuse 7.0-10.0 s · MUTE at 10.0 s · the climb (26 f) and the bloom at 10.9 s · "...Oh." at 11.5 s · the faces wide 12.0-18.0 s · the Mayor's close and the crayon bleed (8 f at 19.0 s) 18.0-21.0 s · R26 · the wipe in 21.9 s, the 60-f split 22.2-24.2 s, the wipe out to 24.5 s · the heart sinks 24.5-28.0 s · the purple firework 28.0-31.5 s · nobody speaks 31.5-37.0 s · the hum 37.0-41.6 s · HOLD 4 s, 41.6-45.6 s · the 5-f iris · black 45.8-48.0 s, then the time cut. Unscored ≈ 33 s, from the MUTE to the cut, less the hum. **Protected:** the final story beat, never trimmed or undercut. The split is D7's 2 s half-split. **Harding:** the firework blooms are ramped (Appendix B already logs 4-08).

**END OF ACT FOUR**

---

## TAG, "Morning"
`20:45-21:20 · 0:35 · 2 beats · SC57-SC58 · cap 12 · used 12`

### SC57 · T-1 · INT. PINK DOOR COTTAGE - DAWN
`20:45-21:00 · 0:15 · cap 0 · used 0`

FADE IN [13 f] on a time cut. MUSIC: the HOME motif on the music box, from frame 0.

Inside, looking at the door. A little pink key turns in the lock, *click*. It's the key from the muffin. The PINK DOOR swings open and wobbles like jelly [JELLY: `TW_WOBBLE` at plush amplitude, 34%], *bwobble*. In the doorway, against the dawn: Sunny, the suitcase, and the pet on Sunny's head.

Sunny's first sight of home. One empty room, a crate and a window. That's all.

Sunny steps in. The door wobbles shut behind them. Sunny sets the suitcase down by the door, *thud*, soft. It stays packed.

[BUTTON +13: behind Sunny, the shut door is still wobbling.]

Sunny crosses to the window, unfolds the tea towel and sets two eggshell halves side by side on the sill. The pet hops down off Sunny's head into one of them, curls up, and turns to face the window [13 f]. Through it, far off in the peach dawn, stands the Wall.

The other half stays empty.

**Timing.** 20:45-21:00 · beat 15.0 s · **no dialogue**. Layout: the fade 0-0.4 s · the key and the jelly door 0.4-3.0 s · the room 3.0-6.0 s · in, the door shut, the suitcase 6.0-8.5 s · the button at about 8.9 s · the shells and the pet 9.5-13.5 s (the 13-f turn ends at 13.5 s) · hold to 15.0 s. The jelly door is canon.

### SC58 · T-2 · INT. PINK DOOR COTTAGE - CONTINUOUS
`21:00-21:20 · 0:20 · cap 12 · used 12`

A KNOCK. On a jelly door it doesn't knock at all: *bwomp. bwomp. bwomp.* [3x@15] The door wobbles with each one.

Sunny opens it. NANA PUDDIN stands on the step with two muffins on a napkin in one paw. Under her arm is her second pie dish, the one that had the blank name card at the Bake-Off. MUSIC: the NANA motif.

> **NANA**  
> *(warm, at the door)*  
> Morning, sweet pea. <!-- t2_nana_04 -->

One each: one muffin goes into Sunny's paws, and she sets the other on the sill beside the pet's eggshell.

Sunny holds theirs up to one ear and gives it a careful shake.

> **NANA**  
> *(amused; she knows)*  
> Just muffin. <!-- t2_nana_05 -->

She holds out the pie dish. It has been scrubbed clean. In the middle of it sits one cookie, burnt nearly black.

> **NANA**  
> *(lightly)*  
> Somebody returned my dish. <!-- t2_nana_01 -->

[R13] She doesn't say who. Her thumb rests on the rim of the dish for a moment [R21].

Then she sees the pet. Over the rim of the eggshell, still facing the Wall, poke two tiny horn nubs.

> **NANA**  
> *(knowing)*  
> Ah. <!-- t2_nana_02 -->

[R8]

> **NANA**  
> *(warm, amused, keeping a secret)*  
> ...He's teething. <!-- t2_nana_03 -->

[R21]

SMASH CUT TO: END CREDITS, on the ITS_FINE sting.

**Timing.** 21:00-21:20 · beat 20.0 s · **dialogue ≈ 6.1 s** (12 words at about 2.0 words a second), plus 1.4 s of gaps; the breakdown's talk window is 6 s. Layout: the knocks 0-2.0 s · the door 2.0-3.5 s · "Morning, sweet pea." 3.5-4.9 s · one each 4.9-7.2 s · the shake 7.2-8.8 s · "Just muffin." 8.8-9.8 s · the dish 9.8-11.5 s · "Somebody returned my dish." 11.5-13.5 s · R13 and the thumb (R21) to 14.6 s · the horn nubs 14.6-17.3 s · "Ah." · R8 · "...He's teething." 18.0-19.3 s · R21 · SMASH at 20.0 s. **The new words** ("Morning, sweet pea." and "Just muffin.") come at the door, before the dish line, as the breakdown asks. Nana never says who returned the dish. "...He's teething." is canon (Interlude I), and it is the only time the pet is "he".

---

## END CREDITS
`21:20-21:50 · 0:30 · 1 beat · SC59 · no dialogue`

### SC59 · CR-1 · END CREDITS
`21:20-21:50 · 0:30 · cap 0 · used 0`

Frame 0 is the ITS_FINE sting from T-2's smash. Then comes a music-box arrangement of the theme's chorus hook, so the episode closes on the instrument it opened with.

CARD: the episode's title card from the main title. The pink paint has peeled off it now: MINGLE NIGHT. One last curl of pink hangs off the M. `s_cr1_title`

Then crayon storybook stills of the night, one every 4 s, each dissolving into the next over 13 f (the credits dither dissolve). They have paper grain and boiling lines, and the filter is on. Tonight is history now, and this is how history is told:
1. **The banner:** GIGGLE NIGHT over MINGLE, with the cupcake balloon tied over the N, facing front.
2. **The bubbles:** Sunny and the pet with the Bubble Wand, and bubbles rising with smiling Puff Demons inside.
3. **The Big Puff** bursting into hearts over the BAKE TEN tent: hearts and stuffing, and no red anywhere.
4. **Three candles** on the rail: two out, with curls of crayon smoke, and one burning gold.
5. **A pink heart** over the Wall, with small faces looking up at it.
6. **A purple firework** answering it from the other side.

The music-box hook plays through, then stops one note short of home. It is unresolved on the last frame. The Mingle tune is not heard: it stays unfinished, saved for the finale.

The credits sit in a CREAM window panel at the side of each still, clear of its focal point and never over a face.

**The credits list (draft for the creator).** Every voice line reads "[performer TBD]" until a recorded take replaces all of that role's TTS lines. A role still on TTS when the episode ships reads "Kokoro TTS (synthetic voice)". `show deliver --require human-voice` lists the roles still on TTS.

| Card | Over | Credit lines | Notes |
|---|---|---|---|
| 1 | The title card, 0-6 s | DOOMOGACHI LIFE in HELLvile Park! · "Mingle Night" · Created by [creator credit TBD] · Written by [writing credit TBD] | The creator confirms how they want to be credited. The provenance log (spec §10.8) covers how drafting help is disclosed in the description |
| 2 | Still 1, 6-10 s | VOICE CAST, in order of appearance: MAYOR GIGGS · STEAMY · KEVIN · HATTIE HAHA · MISS LULLA · BONEJANGLES | Mayor: the creator is recommended (breakdown §3.4). Steamy: TTS, or the creator with their own lilt. The rest: TTS until recorded |
| 3 | Still 2, 10-14 s | NANA PUDDIN · PIP · CLINKER · FATHER FUZZ · DR. BLOOM · GURGLE | Pip: the creator is recommended. Nana or Fuzz: the creator's third role. Clinker: stays Kokoro TTS (synthetic voice). GURGLE: one performer, who also performs the purr. The purr is not credited separately, so his name stays off screen in 101 |
| 4 | Still 3, 14-18 s | SUNNY . . . . . . . . . *(nothing after the dots)* · THE PET . . . . . beep. · MASCOTTY . . . . . (nobody knows who) · The Giggle crowd, the fog and the children of HaHaland . . . . . [walla performers TBD] | The three silent roles get gag credits. The walla, the fog and the kids stay Kokoro TTS (synthetic voices), voiced as adults |
| 5 | Still 4, 18-22 s | MUSIC · Main title "Doomtastic Times": music and lyrics [TBD], vocal [TBD] · Score, "HaHaland Day" and "Mingle": [TBD] · Music-box arrangements: [TBD] · The pet's "beep." is the original GBA piezo | The credits follow D4's split sheet. While the vocal is the temp render, it reads "vocal: Kokoro TTS (synthetic voice)" |
| 6 | Still 5, 22-26 s | Animated in the DOOMOGACHI LIFE game engine (PS1) · Voices: Kokoro TTS · *[only if in the mix]* Salamander Grand Piano V3 by Alexander Holm, CC BY 3.0 · *[only if in the mix]* DWSD / freesound.org, CC BY | The two CC BY lines appear only if the mix log says those samples are in the episode (`SELF_MADE_ROADMAP.md` §7) |
| 7 | Still 6, 26-30 s | No citizens of either side were harmed in the making of this episode. · (The demons were.) · HaHaland: Population 500 lovely citizens. · © 2026 Dippinn Productions. All rights reserved. | The population line sets up the post-credits button. The © line waits on the entity (risk R1) |

**Timing.** 21:20-21:50 · beat 30.0 s · **no dialogue**. Layout: the sting on frame 0 · the title card 0-6.0 s · six stills at 6, 10, 14, 18, 22 and 26 s, each crossfading over 13 f · the hook left unresolved on the last frame at 30.0 s. Each card holds about 4 s; the busiest (cards 2 and 3, six names each) need about 2.3 s to read.

---

## POST-CREDITS BUTTON
`21:50-22:00 · 0:10 · 1 beat · SC60`

### SC60 · PC-1 · EXT. THE SEALED GATE - NIGHT
`21:50-22:00 · 0:10 (300 f) · cap 2 · used 2 (captioned babble)`

Night at the sealed gate, from the Hillvile side. SIGN: seven padlocks, KEEP OUT. FOREVER. `s_gate_keepout` The small barred grille, its shutter shut. SFX on frame 0: crickets, and a dry rustle of paper.

[0-90] In from frame left, over the top of the Wall, drifts the LOST PROPERTY lantern. It is unlit, with its crayon tag (`s_a1_10_tag`) and Sunny's pink "1" swinging under it. It sinks, slowly, out of sight behind the gate.

[R13, ~103] A wet CRUNCH. CC: *[wet crunch]*

[~110-158] From behind the gate comes a gurgle so deep the padlocks rattle: once (~118), twice (~133), three times (~148), each rattle bigger than the last.

> **GURGLE** (O.S.)  
> *(three deep, wet swells)*  
> gurgle. <!-- pc1_gurgle_01 -->

CC: *[GURGLE: gurgle.]*

[R13, ~161] BOING. The gate bulges outward once [`TW_BOING`] and settles. The shutter stays shut.

[R8 after it settles, ~180] A small dialogue box appears, whole, with no type-on. Its name plate reads GURGLE.

> **GURGLE** (O.S.)  
> *(a thoughtful gloop; then two syllables)*  
> ...needs salt. <!-- pc1_gurgle_02 -->

The box stays up for 42 f (~180-222). CC: *[GURGLE: ...needs salt.]*

[~225-250] From behind the gate comes a low, satisfied purr, dry and close. *(Non-speaking: GURGLE's performer, who plays Grumbleguts from 102. `PILOT_ROADMAP.md` D8.)* <!-- pc1_grum_01 --> CC: *[low purr]*

[~255] SMASH CUT TO: the DIPPINN PRODUCTIONS card.

The purr's tail is cut dead on frame 0. That hard audio cut is the cut's sound, so this is not a silent smash (Season 1 has none, `GAG_BIBLE.md` §4.7). After it, the card is silent.

[~289-300] On the card's last frame, very small:

> **GURGLE** (O.S.)  
> *(tiny; ends on the last frame)*  
> gurgle. <!-- pc1_gurgle_03 -->

CC: *[GURGLE: gurgle.]*

END OF EPISODE. 22:00:00.

**Timing.** 21:50-22:00 · beat 10.0 s (300 f) · **voice ≈ 2.7 s of captioned babble** (the gurgle under the rattles ≈ 1.6 s, "...needs salt." ≈ 0.8 s, the last "gurgle." ≈ 0.3 s) **plus a 0.8 s purr**; the breakdown's cap is 2 words. Layout, in frames, from the beat sheet: the lantern 0-90 · the crunch ≈ 103 · the rattles at ≈ 118, 133 and 148 · the BOING at ≈ 161 · the box ≈ 180-222 · the purr ≈ 225-250 · the smash to the card ≈ 255 · the last "gurgle." ends on frame 300. There's no score. **Grumbleguts is not seen:** there is nothing in the grille, the shutter never opens, and the only shape is the gate's one bulge. The "1" crosses the Wall, so HaHaland stays at Population 500. His belly rates the snack like everyone else tonight, and that closes the ratings runner.

---

**END OF EPISODE 101**
