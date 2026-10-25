# DOOMOGACHI LIFE in HELLvile Park!
## Episode 101, "Mingle Night": Voice Bible

| | |
|---|---|
| **What this is** | How every voice in the pilot sounds, who can double whom, and which voices are best left to TTS. Built from the script breakdown's voice bible (`BREAKDOWN.md` §3) and updated to the final script (`PILOT_SCRIPT.md`) |
| **Line IDs** | 165 in the episode: MAYOR GIGGS 35, NANA PUDDIN 22, PIP 16, FATHER FUZZ 16, BONEJANGLES 15, CLINKER 12, THE PET 6, STEAMY 5, DR. BLOOM 5, KEVIN 3, HATTIE HAHA 3, GURGLE 3, MISS LULLA 1, GRUMBLEGUTS (purr) 1, walla 22 |
| **Companions** | `sides/<ROLE>.md` (every line of one role, in order, with its cue and a target length) · `RECORDING_GUIDE.md` (how to record at home and where the files go) · `lines.csv` (every line, one row each) |
| **Status** | 25 October 2026, final script. IDs are provisional until the freeze, before the first take; after that they never change |

**Changed since the breakdown.** New lines for the Mayor (5), Nana (7), Pip (2), Fuzz (3, plus two sung hymn phrases), Bloom (2) and Kevin (1, optional); the Mayor's 2-03 line is now two files ("Everybody stay calm! I'll be under here." / "Being calm!"); "The same voice?" / "Same opinions." (`a2_07_fuzz_02`, `a2_07_nana_04`) were cut for time and their IDs retired; the 4-05 relief ripple is `w_giggle_11`. The theme's revised lyric no longer sings the theme line, so the line stays dialogue-only.

## 1 How to use it (for an amateur voice actor)

* **Every role is an adult performance.** All characters are adults except the canon children (Pip; the storybook kids; Miss Lulla's kids), and every child voice is performed by an adult. Any performer can play any role: the briefs below are about pitch, pace and texture, not about the performer's gender.
* **Pitch** is given relative to *your* comfortable speaking range ("top third", "middle", "bottom third"). **Pace** is in words a second (normal speech is 2.5-3). **Texture** is where the voice sits and what it sounds like.
* **Accents:** your own, or a defined fictional lilt. Never a caricature of a real accent, and never an impression of a real person (hard limit 4; `GAG_BIBLE.md` §8.9).
* **Codes in the text:** `{d}` = deadpan (flat, no smile, no music under you); `{w}` = wavy (the fog); `{big}...{/}` = the word lands whole; a word in CAPITALS is stressed, not shouted (except Pip's shout); `...` at the start of a line = you come in a little late, small.
* **Two sample reads** per role are taken from the locked script, so you can audition yourself today. Record them with the naming in `RECORDING_GUIDE.md` and drop them in as `scratch:` takes.
* **Pronunciation (proposed; confirm, then it goes into `series/lexicon.shw`):** HaHaland HAH-huh-land (the theme sings it that way) · Hellvile HELL-vyle · Hillvile HILL-vyle · Gigglesworth GIG-ulz-wurth · Giggs GIGZ · Puddin PUD-in · Bonejangles BONE-jang-gulz · Clinker KLINK-er · Lulla LOO-luh · Mascotty mass-KOT-ee · Doomogachi doom-oh-GAH-chee · Dippinn DIP-in · Mingle MING-gul.

## 2 The cast, with codes and temp TTS voices

These are the codes used in line IDs and in the takes folders, and the stock Kokoro voice each role uses until you record it (`show voice audition <code>` plays it). A role you record replaces its TTS line by line, by ID.

| Code | Speaker | Role | Temp TTS (`SHOW_ENGINE_SPEC.md` §10.3; auditioned with `show voice audition <char>`) |
|---|---|---|---|
| `mayor` | mayor | Mayor Giggs | `am_puck` 0.6 + `am_michael` 0.4, speed 1.00, +2 st; "Hee. Hee." is a sung library take |
| `nana` | nana | Nana Puddin | `bf_emma` 0.88, −2 st |
| `pip` | pip | Pip | `af_sky` 1.30, +3 st |
| `fuzz` | fuzz | Father Fuzz | `am_liam` 1.10, +2 st |
| `bone` | bonejangles | Bonejangles, himself and possessed | `bm_george` 0.95, −2 st; possessed `flatten_f0=0.7` |
| `clink` | clinker | Clinker (voice only) | `af_nicole` 0.85, −3 st, `flatten_f0=1.0`, `late=R8` |
| `bloom` | bloom | Dr. Bloom | `af_aoede` 0.95, +1 st |
| `steamy` | steamy | Steamy | `bf_lily` 1.00 with a defined fictional intonation contour (WORLD); never a non-English voice to fake an accent |
| `kevin` | kevin | Kevin | `am_onyx` 0.92, `flatten_f0=0.5` |
| `hattie` | hattie | Hattie Haha | `bf_isabella` 0.85 |
| `lulla` | lulla | Miss Lulla | `af_heart` 0.90, −2 st, −6 dB tent filter |
| `gurgle` | gurgle | GURGLE | none: `VOICE_GURGLE` + the Grumbleguts performer's bass babble, tuba, draining sink |
| `grum` | grumbleguts | Grumbleguts (the purr only, non-speaking in 101) | `am_fenrir` 0.7 + `am_santa` 0.3 (a purr is better recorded) |
| `pet` | pet | the pet | none: three raw piezo files, `process=none` |
| `w_<group>` | `walla:<bed>` | walla | `am_eric`, `af_river`, `am_adam`, `af_kore`, `bm_daniel`, `bf_alice` (±2 st by seed); storybook kids `af_sky`, `af_river`, `am_puck`, `bf_lily` +4..+6 st; the fog's child "us" `af_kore` +6 st (not `af_sky`, which is Pip) |
| `ad_<beat>_<nn>` | ad | audio description narrator (in `ad.shw`) | `bm_fable` |

## 3 The speaking roles (12)

### MAYOR GIGGS (he) · "Mayor Gigglesworth" · playing age 40s
* **Who:** a round cherub with busy little wings, top hat, pompadour, handlebar moustache, bow tie. The town's showman and its chief non-looker. "Hee. Hee." is the sound of a man not looking at something. Cheer is his lid; he panics theatrically ("Being calm!"), and once, at the end of Act Three, he is quiet and means it.
* **Pitch:** upper-middle to top third; a musical-theatre tenor's ring; the smile is audible on every line.
* **Pace:** patter, 2.7-3.0 words a second, every sentence a curtain call. His **FREEZE** stops are dead stops: he doesn't trail off, he just stops, mid-breath.
* **Texture:** bright, forward, crisp consonants, sing-song contour (big rises and falls); a little breathless flutter when he's excited.
* **Catchphrases:** "Hee. Hee." (the identical library take) · "Enemies don't exist!" · "Lovely, lovely weather!" · the hum of the Mingle, which always stops on the same note (until 4-08).
* **Watch for:** the theme line in 1-08 is `{d}`: no wink, no smile; the joke is that he means it. "Father. Nobody on this side says those names." is his one real quiet register: low, level, almost kind.
* **Sample read 1** (`a1_08_mayor_01`, bright patter, about 4 s): "Welcome, welcome! Mayor Gigglesworth. Friends call me Giggs. Enemies don't exist!"
* **Sample read 2** (`a3_07_mayor_01` + `a3_07_mayor_02`, quiet, R13 between): "Father." ... "Nobody on this side says those names."
* **In the final script: 35 line IDs.** 30 are spoken reads; the other five are library takes you record once: the dawn hum (`co12_hum`), the 4-08 hum one note further (`a4_08_mayor_01`), and the one "Hee. Hee." that plays three times (`co13_heehee`, `a1_08_mayor_09`, `a1_13_mayor_01`). So it is 33 recordings in all.
* New since the breakdown: "Ooh, a new face! We get so few." (1-08, canon; the first cut if 1-08 runs long), "Mm! A-plus!" (1-09), "Being calm!" (2-03, now its own file after the tablecloth drops, so you never have to dive mid-take), "Nobody look at the boards!" and "Let's make it OFFICIAL!" (4-05).
* Two gears in 4-05: full showman for the first three lines, then "Old draft." brisk and a beat too quick. In 4-06, "One." / "A small one." / "For the patrol." are three files, each a decision.
* **Scenes:** CO-5, 1-08, 1-09, 1-10, 1-12, 1-13, 2-03, 2-07, 3-07, 4-05, 4-06, 4-08 · **Sides:** `sides/MAYOR_GIGGS.md`

### NANA PUDDIN (she) · playing age about 70 (ELDER)
* **Who:** the sheepkin head baker. Calls everyone "sweet pea", bakes two of everything out of habit, hides things in food, and has told children every kind lie there is. The town's quiet truth-teller: one R8 look a scene, no line on it.
* **Pitch:** bottom third; a warm alto with the laugh at the bottom of the chest.
* **Pace:** slow, 2.0-2.4 words a second; unhurried pauses that hold something back.
* **Texture:** cosy, rounded, a little crumbly; smiling, but the smile can drop out without warning.
* **Catchphrases:** "sweet pea" · "Habit." · "...Mostly."
* **Watch for:** "...Mostly." after the R8 has to be the joke and the confession at once: let the brightness fall out of the voice. "Cl..." (4-01) is a sound that dies in her mouth, not a stutter.
* **Sample read 1** (`a1_09_nana_02`, cosy, amused at herself): "The key's in a muffin. Don't ask which. I forgot. I always forget."
* **Sample read 2** (`a1_09_nana_08` + `a1_09_nana_09`, a bright fib, R8, then the truth slips out small): "The last one hatched into a little demon! Ha! Kidding." ... "...Mostly."
* **In the final script: 22 line IDs.** New since the breakdown: the three bite calls by ear ("Raisin." / "Raisin." / "...That's a button."), "There it is! Mind your teeth.", "And this. An egg." (canon), the 2-02 button "...Six." (to her slice, quietly beating the Mayor's speech), and the tag's "Morning, sweet pea." and "Just muffin."
* "...He's teething." is kindness, a secret she has decided to keep, not a joke.
* **Scenes:** 1-09, 2-02, 2-07, 4-01, T-2 · **Sides:** `sides/NANA_PUDDIN.md`

### PIP (they) · a child (canon; voiced by an adult)
* **Who:** the tiniest bunnykin in town, Junior Festival Patrol sash, Mr. Flops under one arm. Thinks brave means not crying; narrates their own bravery; very official about patrol rules. Their big moment is courage, never anything else.
* **Pitch:** the highest voice in the cast (top of your range, without strain or falsetto squeak).
* **Pace:** the fastest: 1.3x the cast tempo (3.0-3.3 words a second), breathless, sentences that run into each other.
* **Texture:** sincere, bright, a little husky from enthusiasm. Play a kid trying to sound grown-up, never baby talk.
* **Catchphrases:** "Patrol rules." · "My eyes are just doing a rain."
* **Watch for:** the cry in 4-02 is the climax of the episode and has to be real, not cute (the audience laughs at the euphemism, never at the crying). "CLINKER!" is shouted from the diaphragm, dry and alone, last in the session (support it; don't rasp). **If you voice Pip, someone else (or TTS) voices the fog's child "us" in 1-14.**
* **Sample read 1** (`a1_10_pip_01` + `a1_10_pip_02`, officious, proud, quick; R8 between): "Found your one. Lost property goes on a lantern so the owner can see it." ... "Patrol rules."
* **Sample read 2** (`a4_02_pip_01` + `a4_02_pip_02`, crying and trying to hold it together; R13; the second line cracks): "My eyes are just doing a rain." ... "...It's just a NAME."
* **In the final script: 16 line IDs.** New since the breakdown: "I'm the whole patrol." [R8] "And Mr. Flops!" (1-10; the second is Pip's canon line). They set up 4-06, where "For the patrol." is a firework for one kid and one toy.
* Record the 4-06 and 4-08 lines before the cry, and "CLINKER! The name is CLINKER! It says so RIGHT HERE!" dead last: dry, alone, two or three takes at most.
* **Scenes:** 1-10, 2-03, 2-07, 4-02, 4-06, 4-08 · **Sides:** `sides/PIP.md`

### FATHER FUZZ (he) · playing age 40s
* **Who:** the plushie bear priest of St. Cuddles, "stuffed with love and a little bit of buckwheat". Believes nothing is unhuggable. Tonight is the anniversary of the hug that failed. A youth-group leader's energy over a deep well of doubt. His faith is shown with affection, never mocked.
* **Pitch:** upper-middle; high and warm, but rounder and softer than the Mayor (no showman ring).
* **Pace:** quick and eager in the rite (2.6-2.8 words a second); slow and small on the steps (1.5-2.0).
* **Texture:** soft, plush, rounded vowels, no hard edges; earnest, never ironic.
* **Catchphrases:** "THE POWER OF CUDDLES COMPELS YOU!" · "Nothing is unhuggable." · "A demon is a frightened thing in a loud coat."
* **Watch for:** commit completely to the battle cry with no wink. Then "It didn't work then either." with nothing left: don't perform tears; let the line sit in the protected hold. The hymn is clapped and sung on the 140 bpm grid (see the final-script note below).
* **Sample read 1** (`a2_06_fuzz_01`, full-chested battle cry, arms wide, sincere): "THE POWER OF CUDDLES COMPELS YOU!"
* **Sample read 2** (`a3_04_fuzz_02` + `a3_04_fuzz_03`, quiet, flat with grief; R13): "Eleven years ago tonight, I hugged someone who needed it." ... "It didn't work then either."
* **In the final script: 16 line IDs, two of them sung.** The hymn phrase is "Come in close, come in close, / hold the one who's frightened most.", 4 bars at 140 bpm, clapping on 2 and 4. In 2-06 (`a2_06_fuzz_02`) it drifts late bar by bar until the last two words come out in minor; in 3-07 (`a3_07_fuzz_03`) it holds the tempo and stays in major. If singing isn't comfortable, Kokoro `sing()` renders both.
* New since the breakdown: "Pardon me! Clergy! Coming through!" (2-05), "One's a start!" (2-06), "Gently." (3-07). "Cl-" stops dead on the L with no breath out; record two or three versions.
* **Scenes:** 2-05, 2-06, 2-07, 3-04, 3-07, 4-04 · **Sides:** `sides/FATHER_FUZZ.md`

### BONEJANGLES (he) · playing age 50s
* **Who:** Hillvile's skeleton xylophone teacher, who plays his own ribs. He played the last Mingle. Possessed in front of the whole town; wakes up and gives the breath the last line of its poem.
* **Pitch:** bottom third; low and very expressive.
* **Pace:** medium (2.3-2.5 words a second); rhythmic, a music teacher who can scat in time with rib-clacks.
* **Texture:** resonant and woody, delighted by his own instrument; a little clatter in the consonants.
* **Three modes:** *himself* (warm, musical, "Clack clack!" bright); *possessed* (dead flat, every word the same weight, couplets in strict rhythm; Clinker's voice is laid 8 f under you by the engine, so don't try to lag or double yourself); *waking* ("...Clack?" small and confused; "Something was in there with me." gentle; the couplet completion spoken-sung in time with the hook, warm and in major).
* **Sample read 1** (`a1_08_bone_01`, fanfare-happy, two crisp hits): "Clack clack!"
* **Sample read 2** (`a2_01_bone_01`, possessed, flat, metronomic): "We came on the fog, and we came for the cake."
* **In the final script: 15 line IDs:** 5 as himself ("Clack clack!", "...Clack.", "...Clack?", "Something was in there with me...", the couplet) and 10 possessed (2-01 to 3-07). Possessed lines are recorded to a 140 bpm click, one syllable a click.
* **Scenes:** 1-08, 1-14, 2-01, 2-02, 2-04, 2-06, 2-07, 3-07, 4-03 · **Sides:** `sides/BONEJANGLES.md`

### CLINKER (she) · heard, never seen · playing age 40s-50s
* **Who:** the name behind the breath: a Hellvile gate labourer who writes poems in chalk and rates everyone else's. On screen the breath is only ever "it" or "something".
* **Pitch:** bottom third; low.
* **Pace:** slow and flat (but in the possessed lines you match Bonejangles' rhythm exactly).
* **Texture:** a tired poet at the end of a long shift, with opinions about rhyme. No melody at all. Every number is a verdict.
* **Watch for:** "...Seven." is the only line where she sounds like a person: small, tired, almost pleased, from far away, on time.
* **Sample read 1** (`a2_02_clink_01` + `a2_02_clink_02`, a bored verdict, R8, then flatter): "Six." ... "Needs death."
* **Sample read 2** (`a4_03_clink_01`, soft, warm for the first time): "...Seven."
* **In the final script: 12 line IDs:** 10 possessed twins recorded in sync with Bonejangles' takes, her layer in the 1-14 fog, and "...Seven." (alone, on time). TTS is recommended if you also voice Bonejangles: two timbres sell the doubled voice.
* **Scenes:** 1-14, 2-01, 2-02, 2-04, 2-06, 2-07, 3-07, 4-03 · **Sides:** `sides/CLINKER.md`

### DR. BLOOM (she) · playing age 30s-40s
* **Who:** a ghoul doctor in pastel scrubs, Hellvile-born and hiding it, with cold hands and a dry wit.
* **Pitch:** upper-middle; high and soft. **Pace:** calm, 2.3 words a second. **Texture:** a nurse's calm with something cold underneath; clinical, then the tiniest dry smile.
* **Sample read 1** (`a2_05_bloom_01` + `a2_05_bloom_02`, clinical, R8, dry): "No pulse." ... "He never has one."
* **Sample read 2** (`a3_04_bloom_01`, quiet and urgent, a hand on his arm): "Father, don't."
* **In the final script: 5 lines.** New since the breakdown: "Doctor." (2-05, flat, behind Fuzz) and "...Steady." (3-07, a doctor who didn't expect to find a pulse).
* **Scenes:** 2-05, 3-04, 3-07 · **Sides:** `sides/DR_BLOOM.md`

### STEAMY (she) · playing age 30s
* **Who:** the frogling bath-house keeper. Knows every duck by name, except one.
* **Pitch:** middle. **Pace:** 2.5 words a second, lilting. **Texture:** a cosy sing-song: either a lilt you define (a fixed melody on each phrase: up on the name, down on the period) or your own accent. Never a caricature of a real one; never a borrowed accent.
* **Sample read 1** (`a1_04_steamy_01..04`, a proud list at a quick even pace, then a little wobble of alarm): "That's Duckworth." "That's Quackers." "That's..." ... "I don't know that one."
* **Sample read 2** (`a3_01_steamy_01`, the same tune, smaller, hiding in a duck pool): "...I don't know that one either."
* **In the final script: 5 lines,** as in the breakdown.
* **Scenes:** 1-04, 3-01 · **Sides:** `sides/STEAMY.md`

### KEVIN (he) · playing age 30s
* **Who:** a citizen with three eyes who runs face painting. Nobody ever asks about the third one.
* **Pitch:** middle. **Pace:** 2.5. **Texture:** flat and friendly; a man who has explained the eye many times. Deadpan without being cold.
* **Sample read 1** (`a1_05_kevin_01`, pre-emptive, cheerful-flat): "Yes, three eyes."
* **Sample read 2** (`a1_05_kevin_02`, a touch wistful; don't milk it): "Nobody ever asks about the third one."
* **In the final script: 3 lines.** "It's lonely." (`a1_05_kevin_03`) is new and optional: the first cut if the Giggle crawl is trimmed.
* **Scenes:** 1-05 · **Sides:** `sides/KEVIN.md`

### HATTIE HAHA (she) · playing age 30s-40s
* **Who:** the plushie comedian. Her jokes are about not saying things (the theme line's third use, in Season 3, is hers).
* **Pitch:** middle to bottom third. **Pace:** slow and precise (2.0-2.3), a stand-up's timing: she waits for the laugh. **Texture:** mic-close, dry, warm.
* **Watch for:** "...Yikes." is a whole sentence, after an R26 of silence: small, dry, eyes on the Wall.
* **Sample read 1** (`a1_06_hattie_01`, a pro at work): "Knock knock."
* **Sample read 2** (`a1_06_hattie_02`, relaxed; then later `a1_06_hattie_03`, tiny): "See, that's the trick. Nobody ever says." ... "...Yikes."
* **In the final script: 3 lines,** as in the breakdown. Her "Nobody ever says." gets answered by a knocker in 3-03, with nobody saying who's there.
* **Scenes:** 1-06 · **Sides:** `sides/HATTIE_HAHA.md`

### MISS LULLA (she) · playing age 30s
* **Who:** the sheepkin daycare worker who can nap any baby in under a minute. She tells kids monsters aren't real.
* **Pitch:** bottom third. **Pace:** slow, 2.0-2.4, a lullaby cadence. **Texture:** low, soothing, hushed (inside a tent full of sleepy kids).
* **Sample read 1** (first half of `a1_07_lulla_01`, warm, proud of the kids): "They all drew the same friend."
* **Sample read 2** (second half, softer, a tiny unease she smooths over): "I don't know who he is."
* **In the final script: 1 line,** one file with a 0.27 s pause in the middle (`{..}`).
* **Scenes:** 1-07 · **Sides:** `sides/MISS_LULLA.md`

### GURGLE (it) · Grumbleguts' belly, heard through the sealed gate
* **Who:** always hungry, always honest; it rates the snack like everyone else tonight. Plain text, never deadpan.
* **Performed by the Grumbleguts performer** (one performer for both, always): a bass babble at about 3-4 frames a syllable, processed with `VOICE_GURGLE`, tuba and a draining sink. The words live in the caption; the babble just has to carry their rhythm.
* **Sample read 1** (`pc1_gurgle_01`, the gurgle so deep the padlocks rattle: three long, low, wet swells, each bigger, 15 f apart).
* **Sample read 2** (`pc1_gurgle_02`, "...needs salt.": a thoughtful gloop, a pause, two short syllables and a smack).
* **In the final script: 3 captioned GURGLE events and the purr** (`pc1_grum_01`, non-speaking, D8). One performer for both, always; if you won't play Grumbleguts from 102, leave these to sound design and TTS.
* **Scenes:** PC-1 · **Sides:** `sides/GURGLE.md`

## 4 The non-verbal roles

| Role | What it is | Direction | Two sample reactions / reads |
|---|---|---|---|
| **SUNNY** (they) · a young adult (20s) · the lead | Silent. No lines, no efforts. The only character who can stand perfectly still on purpose. The voice role is cast for Season 2 | Animation and foley only (paw steps, the suitcase, the plush squash of the hug). Audio description carries every Sunny action (section 7) | 1) 1-11: stands perfectly still in front of Mascotty for R21, then takes page three. 2) 4-01: opens their mouth over 8 f, nothing comes, closes it over 13 f |
| **THE PET** (it) · the co-lead | "beep." only: the game's raw GBA piezo, the one sound kept unprocessed; captioned [beep.]; never translated, never named | Nobody performs it. Six gag beeps in 101, at the cap | 1) 2-04: HOLD 21, MUTE 29, the smallest bubble in the show: "beep." 2) 3-07: the metronome, one beep per beat, and the rite holds |
| **MASCOTTY** (she) · adult, identity secret | Silent; no idle clock at all; waves very slowly only to someone who has stood still for R21; thumps 3-then-1 back at the Wall | Foley only (the foot thumps). Never menacing to a kid | 1) 1-11: the paw opens over 8 f; the quarter-speed wave. 2) 1-13: three thumps, then one |
| **GRUMBLEGUTS** (he) · adult · heard, never seen | The purr after GURGLE's caption (D8: wordless, "Mmmh."); his only sound in 101 | A warm, oily baritone's low, satisfied purr, dry and close; slow, amused. Vocal fry, not throat strain | 1) `pc1_grum_01`: the purr, rising slightly at the end, pleased. 2) A shorter alternate, `lib:grum/purr_v2`, for the animatic |
| **THE FOUNTAIN** (SFX) | Two stone faces that giggle 8 f (267 ms) late; once, on time: "...Hee." | A light, bubbly two-note giggle, processed with water; any performer (or the game's HEE_HEE). It is someone laughing on the other side, not the Mayor | 1) `lib:fountain/giggle_v1`: the late giggle. 2) `lib:fountain/hee_v1`: "...Hee." small and on time (1-03) |
| **CROWD WALLA** (adults) | `w_giggle`, `w_tent`, `w_wall`, `w_lulla`, `w_kids` (section 2) | Beds at −18 to −28 dB, panned; scripted lines in unison with a 0-3 f stagger; HaHaland sugar only, no swearing, no words that stand out | 1) `w_tent_01` "Who's there?" in a loose unison, eager. 2) `w_giggle_02` "NOT NOW, PIP." exasperated, all together, staggered |
| **THE FOG** (walla stack) | Many voices plus Clinker's layer; the child echo "...us" by an adult walla performer, never Pip's actor | Whispered to half-sung, slowed, wavy; the second "us" small and too sweet | 1) `w_fog_01` "come and PLAY with us", stretched on PLAY. 2) `w_fog_02` "...us", child-pitched, a beat behind |

## 5 Doubling plan

**If the creator voices everything they can (a solo performer).** Keep each voice in its own cell of the grid, and don't let two roles in the same cell talk back and forth.

| | Fast (≥ 2.6 w/s) | Medium | Slow (≤ 2.3 w/s) |
|---|---|---|---|
| **High** | **Pip** (breathy, run-on, highest) · **Mayor** (ringing showman, sing-song) · **Fuzz** (soft, round, earnest) | **Bloom** (soft, cool, clinical) | |
| **Middle** | **Steamy** (lilt) | **Kevin** (flat, friendly) | **Hattie** (dry, precise, mic-close) |
| **Low** | | **Bonejangles** (woody, rhythmic, expressive) | **Nana** (warm chest, smiling) · **Lulla** (hushed, lullaby) · **Clinker** (flat, no melody) · **GURGLE** / the purr (babble, fry) |

| Plan | The creator voices | Others / TTS | Why |
|---|---|---|---|
| **Recommended start** (the parts worth performing) | **Mayor Giggs** (the most lines; the library "Hee. Hee."; the one quiet line), **Pip** (the climax; the spec recommends the creator for it), and **Father Fuzz** or **Nana** | TTS for the rest; Clinker, the fog, all walla and both kids' groups on TTS | Three performances carry the episode's comedy and its heart; everything else can ship as TTS and be swapped later by ID |
| **Solo, maximum** | Mayor, Pip, Fuzz, Nana, Bonejangles, Kevin, Hattie, Lulla, Bloom, Steamy (only with your own accent or a lilt you can hold), and GURGLE + the purr **only if you'll also play Grumbleguts from 102** | **TTS: Clinker** (her slip under Bonejangles reads as two people only with two timbres), **the fog's child "us"** (must not be Pip's performer), the walla beds (or stack your own reads 4-6 times at different pitches), the storybook kids | Every speaking role but Clinker is reachable by one performer with the grid above |
| **Two performers** (the creator + one more) | The creator takes the high and middle rows (Pip, Mayor, Fuzz, Bloom, Kevin, Steamy) | The second performer takes the low row (Nana, Lulla, Hattie, Bonejangles, Clinker, GURGLE/purr) | Every same-scene pair below is then split across two people |
| **Best left to TTS whatever happens** | — | Clinker (processed flat anyway), the fog stack, walla beds, storybook kids, Lulla's kids, and the AD narrator if the creator prefers | They are processed, stacked or mixed low; temp TTS is close to final for them |

**Same-scene pairs in the final script** (the beats where both roles speak). If one person doubles both, keep them in different cells of the grid:

| Pair | Beats | How to keep them apart |
|---|---|---|
| Bonejangles / Clinker | 1-14, 2-01, 2-02, 2-04, 2-06, 2-07, 3-07, 4-03 | Always together: two timbres needed, so Clinker on TTS or a second person |
| Mayor Giggs / Pip | 1-10, 2-03, 2-07, 4-06, 4-08 | Keep Pip higher and faster, the Mayor more melodic |
| Dr. Bloom / Father Fuzz | 2-05, 3-04, 3-07 | Both high: make Bloom slower and cooler, or put Bloom on TTS |
| Bonejangles / Father Fuzz | 2-06, 2-07, 3-07 | Easy: low and flat against high and earnest |
| Bonejangles / Mayor Giggs | 1-08, 2-07, 3-07 | Easy: low and woody against a bright tenor |
| Clinker / Father Fuzz | 2-06, 2-07, 3-07 | Clinker on TTS |
| Bonejangles / Nana Puddin | 2-02, 2-07 | Both low: Bonejangles possessed is flat and metronomic, Nana warm and smiling |
| Clinker / Mayor Giggs | 2-07, 3-07 | Clinker on TTS |
| Clinker / Nana Puddin | 2-02, 2-07 | Clinker on TTS |
| Father Fuzz / Mayor Giggs | 2-07, 3-07 | The Mayor's 3-07 lines are his quiet register, which helps |
| Mayor Giggs / Nana Puddin | 1-09, 2-07 | Easy: high and bright against low and slow |
| Dr. Bloom / Bonejangles | 3-07 | Easy |
| Dr. Bloom / Clinker | 3-07 | Clinker on TTS |
| Dr. Bloom / Mayor Giggs | 3-07 | Bloom slow and cool, the Mayor bright |
| Bonejangles / Pip | 2-07 | Easy |
| Clinker / Pip | 2-07 | Clinker on TTS |
| Father Fuzz / Nana Puddin | 2-07 | Easy: Fuzz high and quick, Nana low and slow |
| Father Fuzz / Pip | 2-07 | Pip faster and higher; Fuzz rounder and softer |
| Nana Puddin / Pip | 2-07 | Easy: the highest voice against the lowest |

**Hard rules for the doubling plan:** GURGLE and Grumbleguts are always one performer. The fog's child "us" is never Pip's performer. A hired performer's voice is never cloned by TTS (the no-digital-replica clause); TTS uses stock voices only.

**The plan this pilot recommends (a solo performer starting out).**

| Step | Record | Lines | Leave on TTS for now |
|---|---|---|---|
| 1 | **Mayor Giggs** (the most lines, the library "Hee. Hee.", the one quiet line) | 35 IDs, 33 recordings | Everyone else |
| 2 | **Pip** (the climax; the spec recommends the creator for it) | 16 | |
| 3 | **Father Fuzz** or **Nana Puddin** (the heart) | 16 or 22 | |
| 4 (optional) | Bonejangles, Kevin, Hattie, Lulla, Steamy (with your own lilt), Bloom (if she stays apart from Fuzz) | 15 + 3 + 3 + 1 + 5 + 5 | Clinker, the fog, all walla, the storybook kids, GURGLE and the purr (unless you'll play Grumbleguts from 102) |

## 6 Audio description narrator (optional role)

Audio description is a mandatory deliverable because the lead is silent (`ad.shw`, IDs `ad_<beat>_<nn>`, voiced by the creator by default or `bm_fable`). Neutral, clear, the show's own deadpan for sign gags; every Sunny action described; sight gags described in the hold after them, never over a punch line and never over a protected hold; rewatch clues described as what is there, not as clues. The AD script is written against the animatic, not in this round.

## 7 Session plan for an amateur

1. Warm up for ten minutes (humming, lip trills, a few sirens).
2. Record the library takes first while the voice is fresh: "Hee. Hee." (ten passes), the two hums against the music-box cue.
3. Then by character, in story order, so a voice stays consistent; 3 takes a line; same mic position all session (a fist's width from the mic, slightly off-axis, with a pop filter).
4. The loud and wet things last: the battle cry, the stampede walla, Pip's cry and "CLINKER!", the purr.
5. Water, breaks every 20 minutes, and stop if anything scratches. A voice that hurts is a voice you lose for the week.
6. Name every file by its line ID as you go (`RECORDING_GUIDE.md`); note the take you like in a scratch list, then copy the picks into `takes.toml`.

Full home-recording instructions, the file naming and the drop folder are in `RECORDING_GUIDE.md`.
