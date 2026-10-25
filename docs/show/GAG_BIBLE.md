# DOOMOGACHI LIFE in HELLvile Park!: Gag Bible

**Running gags, recurring bits, Gatekeeper Grumbleguts' gag book, the cute/brutal device, visual gag rules, and the comic timing rules adapted for animation at 24 fps**

| | |
|---|---|
| Version | 1.1 (adult direction), 2026-10-25 |
| For | Writers, board artists, animators, editors, sound, standards, localisation |
| Canon | `docs/design/story/story_final.md` wins every conflict. Timing comes from `docs/design/story/comic_timing.md` (the Comic Timing Bible, reverse-engineered from the 78-second trailer) and `story_final.md` §7 (the timing contract). Grumbleguts comes from `story_final.md` §3 and the Grumbleguts notes (`GRUMBLEGUTS_AND_DIRECTOR_NOTES.md`, `GRUMBLEGUTS_TONE.md`, `GRUMBLEGUTS_ROMANCE.md`), adopted within the hard limits (§7). |
| Companions | `SERIES_OVERVIEW.md` (the season and the engine), `PRODUCTION_ROADMAP.md` (the pilot's production) |
| Rating | Adult animation, **TV-MA** (the pilot TV-MA (V); the series TV-MA (L, S, V) from 102); 18+ elsewhere, 16+ where a territory's scale allows. Not made for kids. Every gag in this document sits inside the show's four hard limits (§7.1, restated in §11.2), which no gag, market or version ever crosses. |

---

## 0. How to use this bible

* **Units.** Every timing is given three ways: the **rung name** (R8, HOLD 13...), the **duration in milliseconds**, and the **frame count at 24 fps**, the count boards, exposure sheets and animators work in. The game and the trailer run at 30 Hz, so `comic_timing.md` counts at 30. The two are reconciled by duration (§2.1), and Appendix A lists both counts for every number in this document.
* **The master frame rate is a room decision** (`SERIES_OVERVIEW.md` §8, D1; open decision D1 in `PILOT_ROADMAP.md`). The roadmap's working spec, and the recommendation, is 29.97 fps, and **the pilot's own timing documents (`PILOT_BEATSHEET.md`, `COLD_OPEN_SHOTLIST.md`) already count at 30**. If the room keeps 29.97, read the 30 fps column of Appendix A. Nothing else in this bible changes, because every gag is specified by rung and milliseconds first.
* **Pixels.** Amplitudes are in **native pixels of the 384x216 show canvas**. Trailer and game amplitudes (240x160) are multiplied by 1.5 and rounded.
* **Notation** in scripts and boards follows `story_final.md` §7 (summarised in §2.5).
* **Precedence inside this bible:** the four hard limits beat everything; then the rest of the standards (§2 of the overview, §7 and §8 here); then timing; timing beats any single gag.

### 0.1 The house style in three words: snap, stretch, stop

`comic_timing.md` names the trailer's rhythm "spastic, elastic, deadpan". That name stays in the game's internal docs. **In everything a writer, buyer or broadcaster sees, call it "snap, stretch, stop"**: "spastic" is a slur in UK English, and the show is pitched to UK and European broadcasters.

| Word | The original word | What it means on screen |
|---|---|---|
| **Snap** | spastic | Fast, sudden, on 1s: entrances in 4-5 frames, smears, smash cuts, shakes, record scratches. Things arrive before you expect them. |
| **Stretch** | elastic | Springs and overshoot, squash and stretch by material, rule-of-three escalation. Everything bounces back, a little too far, then settles. |
| **Stop** | deadpan | Stillness in a world that never stops moving: frozen idle clocks, held frames, silence, a caption that just appears. The stop is the joke, or the scare. |

---

## 1. The comic voice in one page

**The sandwich.** Cute first. Then something is wrong in the background. Then it is funny. Then, sometimes, chosen carefully, it is not funny at all. The pastel world goes wrong in the background, then in your house, then in the sky. Every scare is undercut or doubled by a joke, except the ones that are not, and those are chosen.

**The switch.** HaHaland sees the world through a cheerful filter: a demon pops into confetti and stuffing. The truth is DOOM: blood, gibs, eyeballs, the big red splatter. The show cuts between the two on a beat, with a sound, and **the gag is the switch** (§4.15, §4.16, §8.1). The gore belongs to demons and monsters, and the filter never comes off in a frame with a child in it.

**Who the joke is on:**

* the town's avoidance (the Mayor stepping in front of the camera);
* a character's vanity, appetite, shamelessness or denial (Grumbleguts, the Mayor, Vex's smile);
* the gap between how HaHaland talks and what it does ("Enemies don't exist!"), and between the filter and the truth.

**Who the joke is never on:**

* a kid's fear (we laugh at "my eyes are just doing a rain", never at the crying);
* Hellvile for being Hellvile (the prejudice is the punchline, never its targets);
* the person someone flirts with, or their body;
* consent (a no is never the punchline);
* a child, in any sexual or suggestive joke, ever;
* a real person, a real group of people, a real faith, an accent, a disability.

**The eight laws** (each is expanded later):

1. **Every entrance overshoots** (§4.1).
2. **Gaps sit on the ladder:** 2, 3, 4, 6, 10, 17, 21 frames at 24 fps (§2.1).
3. **Silence is an instrument:** 20-25% of every act has no score (§5.3).
4. **Stillness is the scariest and funniest thing** in a world that never stops moving (§4.18).
5. **An insert is 4 frames,** owns the audio, and comes back to the same shot with one change (§4.16).
6. **Rule of three at a constant interval,** escalating in every channel at once (§4.11).
7. **Setup types, the punch pops, deadpan appears** (§4.13).
8. **Every joke gets a button; the last sincere beat never does** (§4.17).

---

## 2. Timing units

### 2.1 The beat ladder at 24 fps

A rung is a **duration**. The trailer measured it at 30 fps; at 24 fps each rung rounds to the frame count below, keeping every rung distinct.

| Rung | Name | Duration | Frames at 24 | Frames at 30 (game, trailer) | Use it for |
|---|---|---|---|---|---|
| R1 | TICK | 33-42 ms | 1 | 1 | A shutter, a click, a "tik" |
| R2 | BLINK | 67-83 ms | 2 | 2 | The minimum readable gap |
| R3 | REACT | 100-125 ms | 3 | 3 | An instant reaction; a footnote |
| R5 | BEAT | 167 ms | 4 | 5 | Cause to effect |
| R8 | TAKE | 250-267 ms | 6 | 8 | A dawning take; a look to camera; Nana's look at the Mayor |
| R13 | HOLD | 417-433 ms | 10 | 13 | The pause before a deadpan line |
| R21 | PAUSE | 700-708 ms | 17 | 21 | Uncomfortable stillness |
| R26 | DEADPAN | 867-875 ms | 21 | 26 | A double-beat deadpan |

**Reading the ladder:**

* an instant reaction lands at R2-R3 (2-3 frames);
* a dawning one at R5-R8 (4-6 frames);
* a deadpan reply at R18-R26 (14-21 frames).

**Longer holds** used across the show, at 24 fps:

| Hold | Duration | Frames at 24 |
|---|---|---|
| MUTE 13 / 14 | 0.43-0.47 s | 10 / 11 |
| HOLD 18 | 0.6 s | 14 |
| MUTE 29 (the hatch, the music box) | 0.97 s | 23 |
| MUTE 40-41 (the fog's eyes, the starer) | 1.33-1.37 s | 32-33 |
| MUTE 60-64 (the scratch silence, the birds) | 2.0-2.13 s | 48-51 |
| **A protected heart hold** | 3-5 s | 72-120 (default 96) |

### 2.2 Exposure: what goes on 1s and what goes on 2s

| Exposure | Use |
|---|---|
| **On 1s** (a new drawing every frame) | Entrances, impacts, smears, takes, shakes, camera snaps, FX, the hatch, every comic table in §4 |
| **On 2s** (12 drawings a second) | General acting, walks, talk, crowd idle |
| **On 3s** | Only for floaty things: ghosts, fog, Wisp later, the crayon line boil (2 frames per boil step) |
| **A moving hold** | The pose holds but the idle clock runs (breath, blink, ears). This is the default "hold". |
| **A dead hold** | The idle clock is clamped. **Only** for a deadpan, a scare or Mascotty. A dead hold is always a choice, never an accident. |

**Rule:** never hold a drawing longer than 4 frames on screen with the idle clock stopped unless the stop is the joke.

### 2.3 Sound is timed in milliseconds, not frames

Two of the show's most important gags are audio offsets. They are placed **sample-accurate in the mix**, at the same value whatever the frame rate:

* **the fountain's late giggle: exactly 267 ms** after the beat it should land on;
* **a possessed host's second voice: exactly 267 ms** behind the host's own voice (it comes on time only when the breath is home: "...Seven.").

The picture's reaction to either sound lands on the nearest frame (frame 6 at 24 fps, frame 8 at 30).

**The theme's tempo.** The "Doomtastic Times" demo runs at 150 bpm: a beat is 0.4 s, which is 12 frames at 30 fps and 9.6 frames at 24. At 24 fps, cut on the frame nearest each beat and count from the bar line so the error never accumulates (the worst case is half a frame, 21 ms). Confirm against the revised master when it lands in `docs/show/theme/`.

### 2.4 Converting from the game or the trailer

* **A rung converts by name** (R8 at 30 is R8 at 24: 8 frames become 6).
* **A comic table** (POP, SLAM, DROP) is **never resampled.** Keep the extreme poses (the start, the overshoot, the undershoot) and drop the smallest in-between. §4.1 gives the 24 fps tables already converted this way.
* **A spring keeps its damping and its period in milliseconds**; its per-frame coefficients are recomputed (§4.2 gives them).
* **A rate** (pixels per frame, frames between sweat drops) converts by time: multiply per-frame rates at 30 by 1.25.

### 2.5 Script and board notation

Scripts carry the timing codes; boards carry them in the panel margin; exposure sheets carry the frame counts.

| Code | Meaning |
|---|---|
| `[R2]`...`[R26]` | A ladder gap |
| `[HOLD n]` / `[MUTE n]` / `[SILENCE]` | Hold the shot / mute the score (the sequencer keeps running) / no sound at all |
| `[TAPESTOP]`, `[SCRATCH]`, `{r}` | The record scratch (§4.8) |
| `[FREEZE x]`, `[THAW]`, `[PANIC x]`, `[STILL x]`, `[TWITCH x]` | Actor clocks: clamp, release, run at 2x, stop one actor, fast ears |
| `[INSERT n: what]`, `[ONE CHANGE: what]` | The whiplash insert and its persistent change |
| `[TRUTH n: what]` | A truth insert: the same moment as it really is, n frames (always 4), never in a frame with a child (§4.16) |
| `[SMASH]`, `[COVER n]`, `[FLASH]`, `[IRIS♥ out h]`, `[IRIS○ out h]` | Cuts and transitions |
| `[POP]`, `[SLAM]`, `[DROP]`, `[JELLY]`, `[BOING]`, `[BRAKE]`, `[WOBBLE]`, `[SQUASH a/b]` | Entrances and springs |
| `[!RIPPLE]` | A crowd reaction staggered 0-3 frames |
| `[3x@12]` | A rule of three at a 12-frame interval (24 fps; `[3x@15]` in game scripts) |
| `[BUTTON +n: what]`, `[NO BUTTON]` | The after-beat, or its deliberate absence |
| `{.}` `{..}` `{...}` | Pauses of R5, R8, R13 inside a line |
| `{d}` | Deadpan: instant reveal, no bounce, idle clock 0, music ducked or muted |
| `{w}` | Wavy text (the fog, Wisp) |
| `{big}...{/}` | The word pops whole |
| `{punch}` | A punchline: the hold after it cannot be trimmed in the edit |

**Frame-critical flag.** Any panel whose joke depends on an exact count gets a red "FC" in the corner with the count at 24. The editor may not trim an FC hold. The animatic review checks every FC (§11.3).

---

## 3. The twelve rules, adapted for animation

From `comic_timing.md` §0, restated for the show at 24 fps.

| # | Rule | At 24 fps | Show example |
|---|---|---|---|
| 1 | **Every entrance is quick and overshoots.** Pops start at 0.15-0.4x and peak at 1.12-1.35x; slams land from 1.6-2.4x; drops squash to 1.22-1.38 wide. | 4-5 frames to settle | The nine housewarming guests drop in one after another (104) |
| 2 | **Comic gaps sit on the ladder.** | 2, 3, 4, 6, 10, 17, 21 | "Yes, three eyes." [R5: 4 f] only the third eye blinks |
| 3 | **Instant reactions in 2-3 frames, dawning ones in 4-6, deadpan replies in 14-21.** | | "Six." [6 f] "{d}Needs death." |
| 4 | **Silence is an instrument.** 20-25% of each act is unscored. | | The pilot's 21:00: MUTE, R26 (21 f), one cricket |
| 5 | **Stillness is the joke, or the scare.** The world breathes on co-prime cycles, so a stopped clock reads instantly. | Breath 31 f, tail 19, blink every 67, ears every 79 | Mascotty; the possessed; the freeze when Sunny points at the Wall |
| 6 | **A whiplash insert lasts 4 frames,** owns the audio, returns to the same shot with one change. A small sound 2 frames later; the deadpan line at R8. | 4 f insert, +2 f sound, +6 f line | The cupcake balloon turns to face the Wall (101) |
| 7 | **Rule of three at a constant interval** with escalating magnitude, then a topper one HOLD later. | 12 f interval, topper +10 f | Mr. Huggins the 2nd: arm, arm, head (107) |
| 8 | **Deadpan means no bounce, no music, no idle.** The text just appears. | | "{d}Ignore that one." |
| 9 | **Every joke gets a button** 10-20 frames after the punch, often inside the iris hold. | 10-20 f | The crate slides 1 px in shame (102) |
| 10 | **Crowds ripple, never switch.** | Stagger 0-3 f by citizen id | Five hundred heads turn to look at Sunny |
| 11 | **Setup types, punch pops, deadpan appears.** | | Vex's pitch types; "...WAY out of town." appears |
| 12 | **Shake has a profile,** not a random number. | §4.14 | The belly-flop's ROAR (113) |

---

## 4. Technique sheets (24 fps)

### 4.1 Entrances: POP, SLAM and DROP

There is no generic "ease-out-back" in this show. Every comic entrance is one of these tables (scale per frame, 1.00 = rest). Converted from the trailer by keeping the extremes.

| Entrance | Use | Table at 24 fps | Frames |
|---|---|---|---|
| **POP** | Speech bubbles, captions, emotes, the "beep." bubble | 0.40, 1.20, 0.92, 1.00 | 4 |
| **POP, big** | Signs, numbers, a grade, the third eye opening | 0.20, 1.35, 0.90, 1.05, 1.00 | 5 |
| **SLAM** | Shouted captions ("THE DEMONS OF HELL ARE HERE.") | 2.40, 1.50, 0.81, 0.92, 1.02, 1.00 | 6 |
| **SLAM, stamp** | FAIL xN, COSY: B+, TIDY, the Board of Decency sticker | 2.40, 1.60, 0.92, 1.04, 1.00 | 5 |
| **DROP** | Letters, furniture, a guest landing, a suitcase | width 1.38, 1.20, 0.90, 1.03, 1.00 / height 0.62, 0.80, 1.12, 0.98, 1.00 | 5 |

* **The fall before a DROP** is ease-in (t squared) over 5-6 frames, stretched 0.82 wide and 1.25 tall while falling. The contact frame is the last frame of the fall, still stretched; the next frame squashes.
* **Anticipation : action : follow-through ratios** at 24 fps:
  * a high five or a grab: 3 : 5 : 10 frames;
  * an inhale (the possession, Vapula's breath, Grumbleguts sucking in): stretch 8 frames, hold 4 frames with 1 px jitter, release on 1 frame, recover over 6;
  * a press (a paw on glass, a button): 6 : 3 : 8.
* **Micro-anticipation:** 2 frames of a 2 px pull back before any click or toggle.
* **Menus and plain UI** keep the game's slower 12-tick pop. Comic bubbles never use it.

### 4.2 Springs

The trailer's springs, refitted for 24 fps. The damping (zeta) and the period in milliseconds are unchanged; the per-frame coefficients are recomputed (Q14 fixed point, for the TD's procedural layers). Positions follow `e[n+1] = A1*e[n] + A2*e[n-1]`.

| Spring | Use | Period | Zeta | A1 / A2 (Q14) at 24 fps |
|---|---|---|---|---|
| `TW_JELLY` | Letters, the jelly door, plush wobble | 127 ms (3.0 f) | 0.41 | -6159 / -2555 |
| `TW_SLAM4` | "!" bangs | 140 ms (3.4 f) | 0.555 | -2774 / -1351 |
| `TW_SLAM7` | Slam captions | 243 ms (5.8 f) | 0.555 | 7592 / -3899 |
| `TW_SLAM8` | Polaroids, a weapon raised, a lid | 280 ms (6.7 f) | 0.555 | 10428 / -4705 |
| `TW_BRAKE` | The record-scratch camera stop | 280 ms (6.7 f) | 0.45 | 12148 / -6385 |
| `TW_BOING` | **Grumbleguts** (he alone bounces), the window boing | 367 ms (8.8 f) | 0.71 | 12056 / -3883 |
| `TW_WOBBLE` | Furniture wiggle (applied to squash at 0.2) | 267 ms (6.4 f) | 0.22 | 14589 / -10522 |
| `TW_FOLLOW` | A lazy camera follow, a cursor glide | 800 ms (19.2 f) | 0.85 | 18300 / -5699 |

At 30 fps these reproduce the trailer's own coefficients exactly (checked against `comic_timing.md` §3.2).

### 4.3 Squash and stretch

**By material.** Softer and cuter means more squash. Squash always scales around the feet or the contact point, never the centre. Area is **not** kept constant: it dips on the impact frame (to about 0.86-0.95) and is back to 1.0 within 2 frames. That loss of mass is part of the snap.

| Material | Peak deformation | Who |
|---|---|---|
| Candy letters | 38% | Titles, signs, the logo |
| Plush | up to 34% | Sunny, Pip, Fuzz, the pet, every Hillvile critter-folk |
| A plush critter landing | 22% (1.22 wide / 0.80 tall) | Every hop |
| Plastic and hardware | 18-22% | Bonejangles, Rivet's tools, the Pump Shotgun, a handheld |
| Metal | 18% | The gate, the padlocks |
| **Hell** | **0%: hell drips, it never wobbles** | Horns, lava, bone, the red HELL in the logo |
| **Grumbleguts** | **He bounces** (`TW_BOING`); up to 1.4 / 0.7 on a BOING beat only; the rump jiggles one beat (R3) behind the belly on every step and every laugh | The only character who neither drips nor wobbles |
| **Gibs and blood** (the truth) | **0%: they obey hell.** Blood slides on twos; a gib lands with one 2-frame squash and stays where it lands | Only where the truth shows (§8.1) |

**By character** (the model sheets carry these):

| Character | Squash rule |
|---|---|
| **Sunny** | Plush, up to 34%; the ears lag 2 frames behind the head |
| **The pet** | 34% for the plush body; the horns stay rigid (hell); the LCD face never squashes |
| **Mayor Giggs** | 22%; the wings flap on their own cycle; **the halo never squashes, it lags 2 frames** |
| **Nana** | 30%, recovering slowly over 3 frames (wool absorbs) |
| **Pip** | The floppiest: 34%, ears lag 2 frames, Mr. Flops lags 3 |
| **Father Fuzz** | 34%; a hug squashes the person being hugged, not Fuzz |
| **Bonejangles** | Hardware, 18%; the ribs clack on impact |
| **Steamy** | 30% with `TW_JELLY` |
| **Mascotty** | **0%. She never squashes.** She is the stillness. |
| **Hellvile citizens** | Their hell parts never wobble; their soft parts (cheeks, bellies, tails) squash like plush |
| **A possessed host** | 0%, except ears twitching every frame |

**The hop cycle:** 6 frames on an 18 px arc (the trailer's 12 GBA px); stretch to 0.86 / 1.16 on take-off; squash to 1.22 / 0.80 on landing; settle over 3 frames.

### 4.4 Smears

The show never uses motion blur. Blur is off-palette, and off-palette pixels fail the palette lint. Smears are drawn, in palette, at native resolution.

| Smear | How | Example |
|---|---|---|
| **The over-extended in-between** | One frame on 1s, with the moving part drawn about 1.5x too far along its arc (the trailer's devil tail reaches 29, 44, 27 px) | A tail flick, a slap, a head turn |
| **Multiples** | Up to 3 full-colour copies of a limb along the path, on 1 frame (no transparency) | Wing flaps, a spin, a fast wave goodbye |
| **Growth on approach** | Something lobbed at the lens grows fast as it travels (the drip grows from 3 to 20 px in 6 frames) | A thrown cupcake, the stray "1" |
| **Speed lines on the cut frame** | Radial lines that alternate phase every frame for the whole shot | The snap-zoom on Kevin's eye |
| **Sweat drops** | Fleeing citizens throw one behind them every 2nd frame | Every stampede |

**Rules:**

* smears only on 1s;
* never two smear frames in a row, except multiples on a spin;
* **exits accelerate, entrances overshoot**: a fleeing citizen covers x = 3.75f + 0.82f² native px after f frames at 24 (the trailer's flee curve, rescaled), never easing out, each citizen staggered by their id.

### 4.5 Holds

**Before the punch the frame must be readable and still**: the music stops, the camera stops creeping, the walk stops. Idle motion stays on for comedy and goes off for horror.

| Situation | Hold at 24 fps |
|---|---|
| Silence after an impact, before the gag resolves | 8 f |
| Freeze before the reaction | 2-3 f |
| Reaction before the deadpan line | 6 f (R8) |
| A reveal before its caption | 11 f |
| Caption to a tiny wrongness (the button) | 17-19 f |
| A double-beat deadpan | 14-21 f |
| Landing before a decision | 3 f |
| Post-credits beats (pop, strain, plop, horns, beep) | 6, 6, 7, 5 f apart |
| **A protected heart hold** | 72-120 f, no gag |

### 4.6 Freeze-frames

Made by **clamping the actors' animation clocks**, never by pausing the shot:

1. Every clock clamps on the freeze frame; confetti stops in the air.
2. **Anything mid-hop hangs in the air** (cartoon physics) for 11 frames.
3. One frame of shutter flash at low luminance (at most 7/16).
4. Expressions switch to SHOCK 2 frames later.
5. "!" bangs ripple in over 3 frames.
6. 11 frames of musical silence.
7. After the thaw, the clock runs at **2x for panic** breathing.

### 4.7 Smash cuts

| Cut | How | Use |
|---|---|---|
| **Flash cut** | One white frame at reduced luminance (at most 8/16 in the broadcast master) | Mode switches, the snap-zoom |
| **Covered cut** | A cloud, fog or a running citizen covers the frame for 18 frames; the change happens behind it; two red eyes show in the cover first | Any change the audience shouldn't watch happen: a possession, a makeover, the inhale |
| **Insert cut** | §4.16 | The whiplash |
| **The silent smash** | A smash cut with no sound on frame 0 | **Rationed to once per season.** Season 1 has none. Season 2's is F2: "...Close the park." smash to the gate (canon's only silent smash cut). |

**Every other cut has a sound on frame 0.** The audio cuts on the same frame as the picture.

### 4.8 The record scratch protocol

The show's most-used transition. Always in this order:

1. **Picture:** the camera runs at a constant speed (no ease-out) into a sudden stop, overshoots on `TW_BRAKE`, and rattles with `SHK_BRAKE` (+1, -2, +1 px). After that it may creep forward (dread).
2. **Sound:** a tape-stop; the lead slides down 15 semitones over 4 frames (0.16 s), the bass down 12 over 5 frames.
3. **Silence:** 11-51 frames.
4. **The first sound after the silence is small:** "beep.", a *tink*, a single "dun". A big sting only ever comes after a small one has broken the silence.
5. **Re-entry:** the score behaves as if it kept playing silently and comes back on the grid.

**Every rear reveal by Grumbleguts gets a record scratch** (§7).

### 4.9 Double-takes, looks to camera, ripples

* **The classic double-take** at 24 fps: look (6 f), shock with nervous jitter (13 f), stare at the lens (6 f), denial, happy (13 f), re-shock (5 f), then look away and whistle.
* **A look to camera** holds 6 frames straight into the lens before the line or caption.
* **A crowd ripple** staggers each citizen 0-3 frames by a hash of their roster id. Stadium waves run as a phase across columns and rows.
* **A sweat drop** for embarrassment starts 2 frames after the side-eye and slides 1 px every 2 frames.

### 4.10 Camera

* **Integer moves only.** The camera moves in whole native pixels. A slow creep is a stepped move, and the steps themselves read as dread.
* **The creep:** about 2 native px a second (one 1-px step every 12 frames). It always creeps toward the Wall, and the Mayor always steps in front of it.
* **Tilt holds:** the comic camera holds (drifting 1 px every 5-9 frames), then moves fast (10-frame smoothstep). It never eases slowly between subjects.
* **Tilt to the reveal:** 13 frames of smoothstep, then the caption 10 frames later. "The camera finds the consequence."
* **The snap-zoom:** click; 4 frames later, cut to the close-up with a 2-frame flash (8/16, then 4/16) and speed lines; the subject wobbles in y (+9, -5, +2, 0 px); the payoff 3 frames after the cut; the caption 4 frames after the payoff.
* **Irises.** The heart iris is for cute exits, the circle for creepy ones.
  * **Out:** close to the target over 4 frames, hold 3-4 frames (up to 8 if the button needs it) on a detail that moves, then black. The button lives in that hold.
  * **In:** open over 5 frames, slow then whoosh.
  * Sound: a sine "shwip".
* **The brick wipe and the Split:** §8.2.

### 4.11 Rule of three

The canonical example is the trailer's exorcism (s11), and the show's is Sprinklepop's rite in 107.

| Try | At (24 fps) | Gap | Method | Shake | Flash | Counter | Kazoo |
|---|---|---|---|---|---|---|---|
| x1 | 0 | | A spray bottle | STAMP, 3 f | Local white | 2.2x slam | 1 note, falling |
| x2 | +12 | 12 | **A real hug** | STAMP, 3 f | None | 2.2x | 1 note, lower |
| x3 | +25 | 13 | **The whole bucket** | **STAMP3, 4 f** | **Local red** | **2.64x**, plus 16 f of jitter | **3 notes** (5 f, 5 f, 14 f), falling |
| Topper | +35 | 10 | Candles out, SOUL LOST | STAMP | Darken one level per frame | | A minor chord |

**Rules:**

* keep the interval constant (12-13 frames); the rhythm builds the expectation;
* the method escalates toward absurd sincerity;
* the pitch steps down each time;
* **the third is bigger in every channel at once**: duration, colour, scale and number of notes;
* then a **topper**, a fourth and different beat, one HOLD (10 f) later.

**Variants:**

* **List of three and a break:** "Make friends! / Fall in love! / Eat cupcakes!", then "Ignore that one." held in silence.
* **The accelerating three:** the fog's red eyes pop at R8, R5, R3 (6, 4, 3 frames), plus one.
* **Equal steps, then a deflation:** 1000 / 500 / 500, then "1 PARK".
* **Escalating to the personal:** the bath, the bakery, **your bed** (106's chair).
* **List and break by name:** "Sunny... Sonny... Bunny... Toaster."
* **Filter, filter, truth:** two 4-frame truth inserts, then the full switch (101: 3-01's two waves, then 3-02 and 3-03).

### 4.12 Deadpan

* **No bounce, no music** (ducked to 0.5 or muted), **no idle**: the actor's clock is 0.
* **Deadpan text never hops or pops.** It appears, or slides up without overshoot (12, 8, 4, 0 px over 4 frames).
* **The quieter the line, the smaller the text.**
* **Contrast it with a shouted setup.** "THE POWER OF CUDDLES COMPELS YOU!" types with flashing "!" marks; the answer is a tiny, instant "{d}ow."
* **The button is something tiny moving in a still frame**: a slow wave, a blink, a balloon.
* **Ration: at most one deadpan per conversation.**

### 4.13 Text on screen

The show is voiced, but diegetic text carries a large share of its gags: signs, captions, notes, the clipboard, the HUD.

* **Setup types** at about 36 characters a second (1.5 a frame), punch text pops whole (POP table), deadpan appears, shouted words slam.
* **A per-letter hop** on pop-ins: -4, -3, 0, +1, 0 px, two letters starting per frame.
* **Reading time.** Any sign or caption that is the joke stays readable and uncontested for at least 0.3 s per word plus 0.5 s (7 frames per word plus 12). Nothing else moves in its area while it reads.
* **Title-safe.** Everything that carries a joke sits inside 90% title-safe (`PRODUCTION_ROADMAP.md` §4.8).
* **Localisation.** Every diegetic string lives in the string table; the procedural renderer re-renders localised signs, and the textless master carries none. Wordplay gets a translator note (§10.3).

### 4.14 Shake profiles at 24 fps

All shakes use the same sign pattern: x alternates every frame, y every 2 frames (at half the x amplitude unless noted), so it reads as a rattle, not a flicker. Short profiles keep their frame counts; long ones scale by duration. Amplitudes are native px (the trailer's GBA px x1.5). The largest (the shotgun blast and ROAR) stay under 3% of the frame width.

| Profile | Frames | x per frame | Use |
|---|---|---|---|
| `SHK_TAP` | 2 | 0 (y +4, -2) | A caption landing, the soot hand |
| `SHK_BRAKE` | 3 | +2, -3, +2 (no y) | The record scratch |
| `SHK_POW` | 3 | 3 | A slap, a door slam, a clunk |
| `SHK_IMPACT` | 3 | 4 | A splat, the 4-frame insert |
| `SHK_STAMP` | 3 | 4, 3, 2 | FAIL x1, FAIL x2, SOUL LOST |
| `SHK_STAMP3` | 4 | 4, 4, 3, 2 | FAIL x3 |
| `SHK_SLAM` | 4 | 4 | Slam captions |
| `SHK_IGNITE` | 4 | 4 | The logo on fire, the hellify flash |
| `SHK_SHOTGUN` | 6 | 10, 8, 6, 5, 3, 2 | A Pump Shotgun blast |
| `SHK_SAW` | 8 | 3 constant (y 0) | The welded saw's cut (101, 3-05) |
| `SHK_FWOOMP` | 6 | 6, 6, 5, 3, 2, 1 | The possession inhale's release |
| `SHK_CRACK` | 6 | 4, with glitch | The sky cracks; the fountain cracks (S2) |
| `SHK_RUMBLE` | 10 | 4 constant (y +3, -2) | The Wall, a gate under strain |
| `SHK_ROAR` | 11 | 6 x3, 4 x3, 3 x3, 2 x2 | Vapula; the belly-flop |

**Reduced motion** (for the streaming accessibility version, if a buyer asks): shakes become a 1-frame darken pulse on the same frame, so the **timing** of the beat survives and only the amplitude goes.

### 4.15 The brutal register: the truth, hit-stop and the first-person run

The first-person runs (101, 113) are the show's DOOM half: first person, HUD, metal, the hellified palette, and **the truth** (§8.1). HaHaland sees the world through a cheerful filter; the brutal register is where the show stops pretending. **The gag is the switch:** the same hit, first as the town sees it, then as it is.

| Event | Hit-stop at 24 | The filter shows | The truth shows |
|---|---|---|---|
| A Bubble Wand pop (cute only) | 2 f | Hearts and confetti; a *squeak* pop 2 f later | The demon bursts like a water balloon and drops a literal heart, still beating |
| A Pump Shotgun blast (PARTY PUMP / SLAUGHTER) | 2 f | Sprinkles, a sprinkler hiss and a bike-bell ting | Buckshot and red mist; `SHK_SHOTGUN`; the CHK-CHK |
| A demon pops | 3 f | Grape goo and plush stuffing; `SHK_IMPACT` | Viscera, eyeballs and teeth; `SHK_IMPACT`; a gib slaps the lens |
| The welded saw (PURRING SAW / MEATCLEAVER) | 4 f stutter (1 f on, 1 f off, twice) | Ribbons and a purr | A geyser of blood; `SHK_SAW`; the lens painted red |
| A mini-boss stagger | 4 f | A wobble | Chunks missing; `SHK_STAMP` |
| A boss pops | 6 f, plus 1 reduced white frame | A shower of hearts | The full shower and the lens painted red, then the record-scratch silence |

* **Never above 6 frames** of hit-stop at 24: longer reads as a hitch.
* Hit-stop freezes actor clocks only, **not** the HUD face, particles already spawned, or the audio.
* **Muzzle flash is local** (on the weapon only). The full screen never flickers faster than 3 times a second.
* **Every gore beat gets a button**, smaller than the hit: an eyeball rolls to a stop facing the Wall; a tooth *tink*s off the lens; a horn clatters into a bucket.
* **Splatter slides off the lens on twos** and is gone within 2 s; the lens is clean before the next wave's first hit.
* **Saturated red stays local** (the burst, the lens splash). A full-lens red wash ramps in over 8 f or more and drains over 13 f or more, never strobes, and goes on the Harding log (`SERIES_OVERVIEW.md` §2).
* **The weapon plate flips with the register:** PARTY PUMP to SLAUGHTER, PURRING SAW to MEATCLEAVER (the game's own name tables, `core/src/weld.c`), on the frame of the first shot. The Bubble Wand has no brutal name.
* **A hit on Sunny** is a splash of demon blood on the lens and a heart popping off the HUD. Sunny is never seen hurt; the pet is never harmed.
* **The HUD face** (the pet) is the run's reaction shot: a look-around idle, a grin on a pickup, a wince on a hit, a horn nub and glowing eyes at one heart, and a grin at the gore and at the Board of Decency sticker getting shot off (§6.20).
* **Jokes only between waves.** Juice on every hit.
* **Cap: 1:15 of first-person footage per episode** (a cost rule).

### 4.16 Tonal whiplash: the 4-frame insert

1. **The insert is 4 frames** (167 ms at 24 fps). Two frames are too short to read; six are too long to stay a "did I just see that?". **It is never shorter than 4 frames**, in any version: that is the broadcast minimum (subliminal-image rules).
2. **The insert owns the soundtrack** for exactly its length: the score mutes, the insert brings its own sound.
3. **Come back to the same shot with one persistent change.**
4. **A small sound 2 frames later; the deadpan acknowledgement at R8** (6 frames).
5. **Never show a cute character harmed** in the cute half of a whiplash. The gore belongs to the demons and the hell world.
6. **The truth insert:** a 4-frame insert of a filtered moment as it really is (§4.15, §8.1). It owns the audio (a wet shred, a wet pop), it is never cut into a frame with a child in it, and it returns with one persistent change that keeps the truth in the shot (the teacup's inside is red; one cartoon heart on the grass is beating).

Show examples: the cupcake balloon turns to face the Wall (101); the pipe beyond the Wall heaves a purple sigh (101); the teacup (101, 2-03); the hearts that are hearts (101, 3-01); every heart in the wallpaper is an eye, and one stays an eye (106); the IN eyecatch card (every episode).

### 4.17 Buttons, irises and the withheld button

* **Every joke gets a button:** a tiny after-beat 10-20 frames after the punch, smaller than the punch, often a callback. If there is an iris, the button lives in its 3-4 frame hold.
* **Heartfelt is comedy timing with the button withheld.** Hold 3-5 s with no gag.
* **Scary is comedy timing with the release removed.** The same holds, no button, or a release that is a small wrong thing (a slow wave, a head tilt with a creak).
* **The withholding ration:**
  * at most half of an episode's sincere beats go without a button;
  * never two `[NO BUTTON]` beats in a row;
  * **never undercut the last story beat of an episode or an arc.** Its button goes after the credits.

### 4.18 "Nothing is ever static"

All the deadpan and horror tools only work because the world is otherwise alive. Every character runs an idle clock on prime-length cycles, so a crowd never syncs:

| Cycle | Period at 24 fps | From the trailer |
|---|---|---|
| Breathing (1 px squash and stretch) | 31 f (1.3 s) | 39.3 f at 30 |
| Tail wag | 19 f | 22.4 f at 30 |
| Blink (2 frames) | every 67 f (2.8 s) | 3 f every 83 at 30 |
| Ear twitch (left, then right) | every 79 f | 97 f at 30 |

* Hair and ears lag the head (secondary motion).
* Each actor gets a phase offset from their roster id.
* **Possessed:** the clock stops; the ears twitch every frame.
* **Panic:** the clock runs at 2x.
* **Mascotty:** no clock at all.

---

## 5. Density for 22 minutes

The trailer packs a gag every 1.7 s. Twenty-two minutes at that rate would be noise. **Scale the structure, not the density.**

### 5.1 By scene type

| Scene type | Cadence | Notes |
|---|---|---|
| **Set piece** (the hatch, the ratings, the banner, a rite try) | 1 gag per 1.5-3 s | Trailer density, with the ladder and the silence map |
| **Dialogue** | 1 punchline per 3-6 lines | Punch lines pop; one deadpan per conversation |
| **Walk and talk, ambient** | 1 background micro-gag per 20-40 s | A double-take, a running-gag sighting, a prop gag |
| **Decorating** (the dream house) | 1 reaction per placement, then a cooldown | Three visitor reactions 12 frames apart, then a topper |
| **First-person run** | Juice on every hit; a gore button on every kill; jokes only between waves | §4.15 |
| **Heart** | **0 for 3-5 s minimum**, then optionally one soft button | |
| **Horror** | 0: the same holds with no button | |
| **After a big beat** | 5-15 s with no scripted gag | The audience needs to land |

### 5.2 Episode-level targets

* **Across an act:** 8-12 gag events a minute (punchlines, sight gags, buttons, background micro-gags), of which 3-5 are "laughs" (punchlines and set-piece payoffs).
* **Protected holds:** three gag-free holds of 3-5 s in an event episode (the pilot's are Fuzz on the steps, "...Seven." and the firework), at least one in every other episode.
* **The laugh map** (an animatic QA pass): mark every punch on the timeline. Flag any 10 s outside a set piece with more than 3 punchlines, and any 60 s outside a heart or horror window with none.

### 5.3 The silence budget

* **20-25% of every act has no score** (the trailer is at 23%).
* Mutes keep the sequencer running, so music re-enters on the grid.
* Silence goes under the punch, not over the setup.
* **No OMINOUS cue before the fog** in the pilot: the 21:00 fog is the first real scare. Across the series, OMINOUS plays only when an episode means it; a wrongness in the background never gets a sting.

---

## 6. Running gags

**The five rules of a running gag** (from `comic_timing.md` §3.17):

1. **Establish small.** The first "beep." is a tiny bubble.
2. **Repeat soon, nearly identical,** so the audience recognises it.
3. **Then change the context, not the words.** The words stay exactly the same while the stakes escalate.
4. **The last occurrence breaks the frame** (the logo, the post-credits).
5. **One-shot gags come back for the ending.** That is how the story pays off its cast.

### 6.1 "beep."

* **What:** the pet's whole vocabulary. Three forms: "beep." (lowercase, with a period), the **horned** "beep." (lower and rougher, from the first real horn in 106), and "bep?" (a question). The game's piezo motif, the one sound kept raw from the GBA. The smallest speech bubble in the show.
* **Rules:**
  * never translated, never subtitled as a word, never given a name pill;
  * it is the small sting that breaks a silence (§4.8);
  * paired with "...it's fine." it comes R8 (6 frames) before the caption;
  * **at most 6 beeps as gags per episode** (a run of metronome beeps in a rite counts as one), never two within 10 s unless in that rhythm or a rule of three;
  * once a season, the pet may beep during someone's sincere line, as that beat's button, never as the undercut of a final beat;
  * the HUD's pet face beeps in the first-person runs (it is the run's conscience).
* **Season 1 ladder:**

| Ep | The beep |
|---|---|
| 101 | Hatched mid-stampede: HOLD 21, MUTE 29, "beep." ("...Ten."); then the metronome that makes the rite hold |
| 102 | At the grille: the pet beeps, GURGLE gurgles back (the two bellies' first conversation) |
| 103 | Lulla's eleven kids shout "BEEP!"; the pet answers, very quietly, "beep."; and at the paper boat, in love |
| 104 | At the window, turned to face the Wall again, R13 after Sunny turned it round |
| 106 | From the pet bed, during the chair-in-the-bed hold; the first horned beep |
| 107 | Once, very small, at the Bottle |
| 113 | Under the bed, with red light in the window: the season's last line |

* **Later:** demons flinch at it, and the fog beeps back lower (S2); Sunny's first spoken word is its name, answered from across the Wall by a very small "beep." (S2 finale); a hop-beep when it is finally fine (S3); the series post-credits: the poop grows horns. "beep."

### 6.2 "...it's fine."

* **What:** a deadpan caption (or a line) that appears when it clearly isn't.
* **Rules:**
  * it appears; it never types or pops;
  * R8 after its trigger;
  * scored with the ITS_FINE sting (a cheery C chord over a wrong F♯ bass, re-keyed to the theme);
  * at most twice per episode.
* **Ladder:** the pet (101); Nana when the oven smokes (102); possessed Sprinklepop, "I'm fine! {.}{d}We're fine." (107); "IT'S FINE" painted on the church barricade (S2); the Mayor about crossing the gate (S3; it is not; he does it anyway); the title logo on fire: "(it's fine.)" (series end).

### 6.3 "Hee. Hee."

* **What:** the Mayor's nervous tic. Two syllables, a period after each. "The sound of a man not looking at something."
* **Rules:**
  * always exactly two;
  * scored with the Hee-Hee bell (the jingle that also opens every act card);
  * **nobody in Hillvile reacts to it in Season 1**; it is normal there;
  * it is the button on his evasions, at most 3 per episode (the pilot uses exactly 3: the dawn banner, the theme line and the empty sky at 21:00). The S107 speech, which is written "hee. hee." on the page and read aloud as its one sentence, is the speech gag, not the tic, and does not count.
* **Ladder:** evasion (S1); the fountain giggles it late; the fog says it **in his voice** (S2, scary); a cup of tea names it as a wound (S2); "...Hi, Fluffy.", then his first real laugh (S3); "Ha!" [FREEZE ALL] "...Ha?" at Hattie's set.

### 6.4 "We don't talk about the other side."

* **What:** the theme line. Canon spends it **exactly three times** in the series:
  1. **101, as a joke.** The Mayor, deadpan, after "...the weather side." HOLD 13, and the fountain giggles late.
  2. **S2E7, as a confession.** "...That was the whole point. ... I never asked where it went."
  3. **S3, as a punchline.** Hattie opens the festival with it: "Because we're STANDING in it!" and both sides laugh.
* **Rules:**
  * it is never said anywhere else, never paraphrased, never played as a different joke;
  * **it is always spoken, never sung**, and never appears in a lyric (including "Doomtastic Times"). This keeps it from reading as a nod to a well-known song with a similar hook, and keeps the line's weight for its three uses.
* **The Hellvile half: "We don't ask where they come from."** Said once in Season 1 (Ploddy, 111B, as habit) and once in Season 2 (the B-side of S2E7, as discovery: "...So that's where they come from.").
* **The taboo as a visual gag.** When anyone asks about the other side, or Sunny points at the Wall, the town freezes on a tape-stop and every head turns, rippled 0-3 frames. HOLD 21 (17 f). The music comes back on the downbeat with "Hee. Hee." **At most once per arc.**

### 6.5 The fountain giggle

* **What:** the Giggling Fountain giggles **267 ms late** (§2.3). If you stop, R5 later it giggles again, on time: "...Hee."
* **Rules:**
  * picture-wise the fountain's faces don't animate the giggle; the gag is the sound, plus one ripple in the basin on the late beat;
  * **nobody in Hillvile notices**, except Sunny, the pet and Prof. Fizz;
  * at most once per episode, never on top of another punchline;
  * never explained in Season 1 beyond Fizz's question (103): "A two-faced fountain only giggles if someone laughs on the other side. {..}So who's laughing?"
* **The same lateness, everywhere:** every possessed "we" voice runs exactly 267 ms behind its host. It comes on time only when the breath is home. On a rewatch after S2E7 the audience realises the two are the same route.
* **Later:** the fountain cracks (S2E7); after the Wall, the giggle is on time forever.

### 6.6 Population: 500 (and the "1")

* **Established (101):** the sign drops in, "Population: 500 lovely citizens.", updates to 501, and R8 later the "1" falls off (*tink*). Under the 500, a 1000 has been painted over. Pencilled underneath: "and Sunny."
* **Ladder:**
  * 101 post-credits: the "1" drifts over the gate on a stray lantern; a wet *crunch*; GURGLE; "...needs salt."; a purr;
  * **102 cold open:** he burps the "1" up, it lands on his tongue, and he swallows it again: "Mm. {..}Tastes like somebody new.";
  * 103 post-credits: under "and Sunny." someone has pencilled, smaller, "and beep.";
  * 113 post-credits: Grumbleguts pats his belly at the sign: "{d}...Five hundred and one.";
  * series finale: "Population: 1,001."
* **Rules:** the sign reads 500 in every plaza wide shot until the finale (a continuity item on every background); the pencil lines are never erased.

### 6.7 Mascotty, "the Starer"

* **What:** the town mascot in a giant bunny head. "Nobody knows who."
* **Rules:**
  * perfectly still in a crowd that bobs: no breath, no blink, no clock, ever, and no squash;
  * lit by a cool light from nowhere;
  * she waves, very slowly (6-frame phases), only to someone who has stood still for R21 (17 f);
  * "Ignore that one." is always someone else's line, never hers; she does not speak until canon (S3);
  * she thumps 3-then-1 back at the Wall;
  * she is never menacing to a kid: she is a mystery, not a threat;
  * at most one Mascotty beat per act;
  * **she is in every IN eyecatch card of the series, one step closer to camera each episode.** On a 2x11 centre card she is seen from behind, through the loose brick.
* **She never takes the head off** until the canon beat in the rubble (S3). Then: Thumpkin thumps once; she thumps back once. No button.

### 6.8 The camera wants to look, and the Mayor won't let it

* **What:** the camera creeps toward the Wall (one pixel at a time); the Mayor steps into the lens and fills it with a smile. "Hee. Hee."
* **Rule:** once per arc. It teaches the show's grammar: the town is hiding something from the camera itself.
* **Pays:** the opening's final variant pans with no Wall (series finale).

### 6.9 Mr. Huggins the Nth

* **What:** Father Fuzz's practice dummies, each named, each lost.
* **Ladder:** the 1st's grave, "Rest in pieces, Mr. Huggins the 1st" (101); the 2nd, arm, arm, head, which rolls three times (*tink tink tink*) and stops facing the Wall: "{d}Rest in pieces, Mr. Huggins the 2nd." (107); the 3rd's head falls off at the first hug: "{d}They all do that." [R13] "I've stopped asking." (110); the 4th loses an ear (S2); the 6th is torn on the conveyor, a 4-frame insert of real stuffing that comes back as buckwheat (S2, Fuzz's crisis); Sutura mends the 1st through 8th (S3); **the 9th survives and hugs Fuzz** (S3).
* **Rules:**
  * the heads always stop facing the Wall;
  * the loss always lands on the third beat of a rule of three;
  * it is slapstick to a dummy, never to a person, and Fuzz keeps every one "because nobody is practice": comedy with a sad edge.

### 6.10 Nana: two of everything, and food with things in it

* **Two of everything:** two pies, the second with a blank name card ("Habit, sweet pea."); two muffins at the door, one each.
* **Baked into food:** a key in a muffin (101), a map in a pie (S2), a relic in a bundt (S2), a key in the Unity Loaf for a gate that no longer exists ("{d}Habit.", the special).
* **The bite test:** a rule of three, raisin, raisin, "that's a button", then the crunch on the thing.
* **The look:** Nana holds a look on the Mayor for exactly R8 (6 frames), with no line. At most one per scene. She is the town's quiet truth-teller.

### 6.11 "My eyes are just doing a rain."

* **Ladder:** Pip (101, 102); Steamy after the rite (107); the Mayor on the dark night (S2); Tallyworn at his last tally mark (S3). Pip's payoff: "Rain's good for stuff."
* **Rules:** the line always gets an R13 (10 f) hold after it with no gag; the laugh is the euphemism, never the crying.

### 6.12 The crowned cloud

* **What:** a background prop that is always one beat more alive than a prop should be.
* **Ladder:** the main title's last sparkle is purple and sinks (v1); every daycare drawing (101); "Employee of the Month" at Cloud Nine, whose eyes follow you, and whose photo develops empty (105); Vex's crowned-cloud rug under Sunny's bed (106); the cot drawing whose speech bubble says "we" (111); it speaks (113); "Employee of the Month: Vex" (S3).
* **Rules:**
  * **never with a string** (the string belongs to Fluffy, and Fluffy's identity is the series finale);
  * it is never seen to move on camera in Season 1; it has always moved when you look back.

### 6.13 Paper boats

* **Ladder:** a crayon house with a candle in the window, two imps, ARE YOU THERE written backwards (103's cold open); a second boat with an unlit candle and the same house, now lit, plus Pip's note (104); "not in MY room." (S2); "That's MY house. You KEPT it?" (S3).
* **Rules:** boats always arrive on the water from the south, never by hand; always crayon; the hand matches the drawing above the little bed (106).

### 6.14 Kevin's third eye

* "Yes, three eyes." [R5: 4 f] **Only the third eye blinks.**
* "Nobody ever asks about the third one. It's lonely."
* His bust (104) blinks its third eye every 67+ frames, forever.
* **Rules:** only the third eye ever blinks; nobody ever asks; one Kevin beat per episode he appears in.

### 6.15 Rivet: "Don't make it weird." and DAYS SINCE LAST FIRE

* **"Don't make it weird." x3,** escalating, each one faster as she is thanked again: R13, R8, R3 (10, 6, 3 frames).
* **The sign** reads 7; her tail touches a spark; it flips to "-" for 2 frames with jitter, then **0** in red, pulsing every 3 frames. It resets whenever Rivet visits a set.
* **Ladder:** an unseen coat hook with a note, "don't make it weird" (104); on screen at last (108); 0 and on fire, "{d}It's a different fire." (S2); "It's a REALLY good spoon." (S3).
* **"Rip and tear... gently."** waits on counsel (trademark); the plate on the HUD has an alternate.

### 6.16 Grampy's names

* "Sunny... Sonny... Bunny... Toaster." A list and a break. "...where was I?"
* Glasses on over his other glasses: "Better."
* **Rule:** the fourth name is always an object, and never the same object twice in a season.

### 6.17 Hattie Haha

* "Knock knock. Who's there? See, that's the trick. Nobody ever says."
* "Tough crowd. You ARE the crowd." "...Yikes."
* The open mic: a perfect press gets a snort-laugh ripple; a late one gets a cricket.
* **Rule:** her jokes are about not saying things. The theme line's third use is hers.

### 6.18 The unknown duck

* "That's Duckworth. That's Quackers. That's..." [SHOCK] "I don't know that one."
* Every tub has a duck; **one always faces the Wall.** It squeaks back 3-then-1. Steamy names it Guest (103). Guest is in her hand when she is possessed (107), and her possessed scream is a duck squeak, enormous. Guest gains a tiny Hellvile hat (S3).

### 6.19 The ratings: Hellvile counts, Hillvile grades

* **The pilot's runner:** the possessor rates everything. "Six." [R8] "{d}Needs death." / "Four. Too much death." / "Two." (the Mayor's speech) / "...Ten." (the first beep) / "...One." (THE POWER OF CUDDLES) / "Three. Needs an ending." / and, from beyond the Wall, on time: "...Seven." The last rating is the emotional peak of the episode.
* **The series rule:** Hellvile rates with numbers out of ten; Hillvile grades with letters and stamps (COSY: B+, TIDY). It is a culture gag that also tells the audience, quietly, that a demon is a person with opinions.
* **Grumbleguts inherits it:** "Six. Needs salt." The pilot's post-credits caption ("...needs salt.") ties the two together, and his first ratings in his own voice are 102's: Nana's muffin, "{d}...Seven.", and the Small Puff, "Six. {..}Needs salt."
* **Rules:** always an integer; said flat; the tail is always "Needs X."

### 6.20 The Board of Decency always loses

* **What:** the HELLVILE PARK BOARD OF DECENCY (approved), chaired by the Mayor (members: Oma Fluff, Ribbonetta and, by co-option, a doily), is HaHaland's censor, and it always fails. It is the town's covering habit made into a committee: GIGGLE painted over MINGLE, a cupcake balloon tied over a word, the Mayor's pink roller.
* **Its sticker appears at most once a season, and only when the sticker itself is the joke.** Season 1's is in 101 (3-03): it slaps over GORE: BRUTAL on the HUD, and the first shell blows it off in a spray of red; it flutters down the lens, and the label stays BRUTAL.
* **Everywhere else the Board covers things the way the town does, and the cover fails on the third try.** In 102 the Board covers the grille with a doily (eaten), the cupcake balloon (popped with a fang) and the GIGGLE banner (licked: "Strawberry."), then paints it pink, and a tongue licks a clean peephole. On the logo, the Mayor's pink roller paints over the red drip, and on the last chord the drip bleeds back through.
* **Rules:**
  * never a censor bar, never a blur, never a sticker over anything explicit, because nothing is (hard limit 1);
  * the cover is the joke, and it always loses;
  * at most one Board beat an episode.

### 6.21 The little bed (the Season 1 home runner)

* A tiny imp-sized bed nobody placed appears in the corner (106). It moves **one cell closer to Sunny's bed between episodes**, never on camera, checked in each tag: a lamp turned toward it as a night-light; a "DO NOT SELL" sign made from Sunny's shopping list (terrible handwriting, worse spelling) [R8] "beep."; a drawing of a house with two windows.
* By 113 it touches Sunny's bed, and the dream asks "is this your room? it's nice. can i stay?"
* **Pays:** "not in MY room." (S2).

### 6.22 Who says the name (the sincere runner)

Not a gag, and the bible guards it like one. Each rite, a different Hillvile citizen breaks the taboo (`SERIES_OVERVIEW.md` §4.3).

* **The name is never comedic.** No button on it, ever.
* It gets an R13 hold before and after.
* It can be a whisper.
* The Mayor's reaction shot is its B-side, one step further each time: he stops it, he flinches, he looks away, his lips move.

### 6.23 GURGLE

* **What:** Grumbleguts' belly, which speaks for itself. **It.** Always hungry, always honest, sides with the audience about half the time.
* **Rules:**
  * GURGLE's lines are plain text, never deadpan; big gurgles get a record scratch;
  * captions read **[GURGLE: gurgle.]**;
  * Grumbleguts translates, and GURGLE contradicts him when he's lying ("...He says eight.").
* **Season 1:**
  * 101 post-credits: a gurgle so deep the padlocks rattle three times, then BOING, then "...needs salt.";
  * **102:** heard and seen at the grille: the two-belly conversation with the pet, "He says you're cute. {..}He's not wrong.", and the contradiction ("...He says I'm blushing.");
  * 103-107: at the grille, one beat an episode;
  * 108-112: **one distant GURGLE per B-segment** rattles the cups on a Hellvile shelf, and nobody comments, plus his cameo;
  * 113: the warning gurgle before the belly-flop, and "He says hello."

### 6.24 Small runners

| Runner | First use | Rule |
|---|---|---|
| The clipboard act cards (and every episode's own piece of paper) | 101 | The act card is always a document from the episode |
| GIGGLE painted over MINGLE | 101 | The wet N bleeds back through; a town painting over a word it can still read |
| "...Mostly." | 101 | Nana's one-word footnote; never anyone else's |
| "Clack clack!" | 101 | Bonejangles; a hug plays a chord |
| "NOT NOW, PIP." | 101 | The whole crowd, in unison; Pip's question is always the right one |
| "Sorry. Ghoul." | 106 | Bloom, then the instant cover: "I mean: *cool* hands." |
| Hauntessa's notes ("your crate is a table now. you're welcome.") | 103 | Lowercase, tidy, apologetic |
| "It's a texture." | 109B | Charlotte, about anything burnt |
| "Present." | 109B | Drudge, once, and never again |
| "Nobody's practice." | 110 | Said on both sides of the Wall in the same episode |
| "Habit." | 101 | Nana; later Tallyworn and Signhere's world (S3) |
| The OPEN LATE sign | 102 | The gate's Hellvile face; KEEP OUT. FOREVER. faces the other way |
| The grille | 101 (shut), 102 | A small barred window in the sealed gate. The town sees him only through it until 113; it gets painted pink and licked clean |
| "Nobody knocks." | 102 | Pays in S3: "Kept this gate ten years. Nobody came up to see it." |
| "Didn't happen." | 102 | After every kindness he does |
| The butt-print signature | 102 | On every document an adult hands him (never one from a child: rule 3) |
| "gift from G" | 104 | A Gate Key (Copy) on a ribbon, tagged, on the cottage shelf forever ("Doesn't open anything on your side. {..}It's the thought.") |
| SUITE CLOSED | 102 | The gatehouse sign; the building rocks; a stud pings off the roof |
| "Rest in pieces" | 101 | Graves and dummies only |
| The Plain Human figurine | Dream house | The only decor that never animates, ever |
| Junk mail "from The Other Side" addressed to "THE SNACK, Pink Door Cottage" | S2 | "Well. It's a valid address." |

---

## 7. Gatekeeper Grumbleguts: the gag book

He is the show's breakout character and its biggest standards risk. Both are true at once, so his rules come first.

**Who he is.** Hell's biggest flirt and its biggest appetite: a flamboyant, shameless, promiscuous, smooth-talking bad influence in the Red Guy mould, with the energy of an Angel Dust-type adult-cartoon demon (a *type*, never a template: no borrowed design, name or line). A slow drawl, hooded eyes, a purr in the voice and a breathy pause before every punchline. He performs for the room with his jokes and his body, propositions every adult on both sides of the Wall, gets lucky off-screen, and gets turned down by everyone on this side. Under the act he is lonely and loyal, and every filthy joke is one step from a brave thing he will swear he didn't do. Canon: `story_final.md` §3, `GRUMBLEGUTS_AND_DIRECTOR_NOTES.md`, `GRUMBLEGUTS_TONE.md` and `GRUMBLEGUTS_ROMANCE.md`, adopted here within the show's four hard limits.

### 7.1 The rules (binding)

1. **The show's four hard limits** (`SERIES_OVERVIEW.md` §2). Nothing below loosens them.
   1. **Nothing explicit.** No explicit sexual acts and no genitals on screen, ever. Sex is implied, off-screen or innuendo: a closed door, a sign, a sound, a reaction, a smash cut. His design has nothing explicit to hide: his front is smooth cartoon with no anatomy drawn, and a bare cartoon butt and the cartoon X pucker are fine.
   2. **Adults, and consent.** Every sexual or suggestive moment is between adults and consensual. A no ends the advance on the spot. Nobody's body is used on someone who has not said yes. Silence is never a yes. Consent is never the punchline.
   3. **Children are never in it.** Child and teen characters (Pip; the young cherub and the young imp in the storybook flashbacks; Miss Lulla's daycare kids; Bolt, Pokefork, Wisp, Tantrum, Wobbles, Homework; every KID- or TEEN-flagged citizen) are never in sexual or suggestive scenes or dialogue, and never the subject of graphic gore.
   4. **No real-world hate content.** No slurs, no real-world hate symbols, and no joke whose target is a real group of people, a real faith or a real person.
2. **He flirts with every adult in both towns:** Nana, Father Fuzz, the Mayor, Charlotte, Anvil, Rivet, Mother Nox, Hattie, Dr. Bloom, Sutura, the forklift driver, his exes, and every adult who comes to the gate; plus the Gate, objects (a napkin, a bagel, the grille's bars) and his own reflection. **Sunny** is an adult and may be flirted at (`PILOT_ROADMAP.md` D13). In Season 1 Sunny always gives a visible no (ears flat, a slow headshake, a paw up, the pet beeping at him) and he steps back. Sunny's silence is never read as a yes, by him or by the staging. No romance arc for Sunny in Season 1.
3. **Kids: the grumpy uncle, and nothing else.** A scene is one continuous time and place. In a scene with a child or teen in it there is **no innuendo anywhere in the scene, his rear stays out of frame and out of the joke, and he bites back the swearing**: "Go home. Eat something. {d}Not THAT." (the THAT is always visibly food, or an inedible thing the kid is reaching for). Where canon puts kids and a flirt together (the church window, game Ep 3.1), the show splits it into two scenes, with a cut and a time jump between them.
4. **A no ends it.** A no can be said, signed, beeped or shown (ears flat, a step back, a paw up, a door shut). He steps back on his next line ("Shy. {..}Noted.", "Fair.", "...I respect that more.") and never repeats the advance in that scene. A line that misreads the no as a game (canon's "Playing hard to get.") is always answered on screen ("{d}I'm not playing.") and taken back on his next line, so the joke is his vanity and the no is never left standing as a game. He never corners anyone, never touches without a yes, and never sits on anyone who hasn't invited it. The no is the running gag, and the joke is always on him, never on the person who said it.
5. **His hookups are off-screen and implied, with willing adults who came to him.** The visitor's voice is the invitation ("You said 'open late.'"); then the gatehouse door shuts, the sign flips to SUITE CLOSED, the gatehouse rocks on its foundations, a harness stud pings off the roof, and a satisfied voice leaves ("Call me." "I won't."). That is all anyone ever sees or hears. On screen, the Hillvile cast turns him down: nobody on this side is interested, except possibly the Gate.
6. **The joke is his vanity, appetite and shamelessness**, never the target's body, size, shyness or no. He rates food out of ten. He rates himself eleven.
7. **Belly and appetite are open:** eating with enormous effort (two-handed, cheeks puffed, a groan of pleasure on every swallow), the gut sucked in and sprung out, studs and buckles pinging, the belt unbuckled with a dramatic sigh, rating food, wet burps, drool, and gas both ways (burp and fart as tempo-synced chiptune raspberries with a vinyl-crackle tail; cute green puffs with hearts in them under the filter, sulphur-yellow clouds with flies in the truth). **Gross-out close-ups at most 2-3 an episode**, so they land (smash cut, HOLD 13, record scratch), in the Ren & Stimpy spirit of the director's notes. Gas and gross-outs never play in a scene with a child.
8. **The butt is open:** bare, big, smooth, peach-shaped and always somehow in frame (the Red Guy rule). Strut-aways camera-side, backing into frame, doors butt-first, the bend, the wiggle, reflections, the butt-print, sitting on things, the X pucker, the blush. Close-ups are allowed when they are the joke. Nothing anatomical is drawn between the cheeks: **the cartoon X is the only mark**. **Every rear reveal gets a record scratch** (§4.8).
9. **No censor stickers** (§6.20). The hard limits mean there is never anything explicit to hide, so his gags are simply shown. When a moment has to stay implied, staging does it: framing, props, the grille's bars, silhouettes, smash cuts, reaction shots, off-screen sound.
10. **The edgy antihero.** After a filthy joke he does the brave thing. He drops the act to save a friend; the save is played straight and never undercut, and the denial comes after ("Didn't happen.", "Did it for the snack.").
11. **Language.** He swears in the R range (damn, hell, ass, bastard, and an f-bomb at most once an episode, so it stays a punchline), timed like any other punchline. Never a slur (hard limit 4); never in a scene with a child.
12. **Never mean**, to him or from him. He is crude on purpose and proud of it, and he never punches down. The show loves him.

### 7.2 The model

From `story_final.md` §3, the director's notes, the concept sheets drawn by `trailer/concept/grumbleguts.c` and roster row 729. Commission the model sheet now: 101's main-title silhouette and 102's boards both draw from it.

* **Body:** a glossy red pot-bellied imp devil with tall black devil horns, fangs, lashes, a beauty mark and an enormous grin; bat wings; a thick spiked tail; gym-demon muscle over the gut (the shoulders and arms carry the effort); Baloo's loose, belly-forward posture; a peach belly and peach cheeks.
* **The hell dress code:** spiked collar, leather chest harness, studded belt and cuffs, chains, **the gate key on a chain** (it swings as a flail in the Season 3 fight), and **a riding crop**, his gatekeeper's tool: he taps the bars with it, points with it, and conducts the Gate's creak with it. Optional: a leather peaked cap as his badge of office.
* **The butt:** bare, smooth, peach-shaped, about a third wider than the belly seen from behind. **The front is smooth cartoon, with no anatomy** (the Red Guy rule).
* **Overlays:** the **X pucker** (a 5 x 5 native px cartoon X between the cheeks; it pops in on 1s with a *tink*, holds R3 and is gone on the next beat) and **two blush layers** (the face's cheeks and the seat's, which glows pink through the grille at night).
* **Material:** glossier, rubberier and sharper-lined than the plush cast. **He bounces** (`TW_BOING`, period 8.8 frames at 24): squash up to 1.4 / 0.7, on a BOING beat only. **The rump jiggles one beat (R3) behind the belly** on every step and every laugh.
* **Scale:** the gate's grille is 48 x 32 native px and his rump is 64 px across. It never fits, and that is the joke.
* **Turnarounds to board first:** rear three-quarter ("my good side"), the bend, the over-the-shoulder look, his face filling the grille, the strut-away cycle (8 drawings on 2s, the rump a beat behind), the belly-flop, the blush on both sets of cheeks.

**What the game's concept sheets give the show** (`docs/design/story/concept/`): the scale shot against a citizen chibi, the reaction cut with the eyes bulging one at a time, the belly X-ray ("60% air, 40% bagel"), the flirt and unbuckle poses, the taunt turnaround and the gas puffs. **Not used:** the censor sticker as a cover (§6.20), and the Feast storyboard's hose close-up as drawn (§7.6).

### 7.3 Two voices in one box (one performer)

* **GRUMBLEGUTS** (he): a warm, oily baritone, slow, purring and amused, with a breathy pause before the punchline. He swears in the R range (rule 11) and he sings: his number "Open Late" comes in Season 3, and 102 has its bridge. Sign-off: **"Open late."** (the series' pick; "Sulfur kisses!" stays Vex's).
* **GURGLE** (it): the same performer's bass babble, processed with tuba and a draining sink, about 3 frames a syllable at 24 fps. Plain text, never deadpan. Captioned [GURGLE: gurgle.]. See §6.23.
* **His laugh** is the purr breaking: a low "heh" that the belly finishes.
* **Pet names (adults only):** sugar, darlin', hon; "padre" for Fuzz; "Mayor", said like a pet name. Never "kid" or "new kid" in any flirt; for Sunny, "bunny" or "newcomer".

### 7.4 The gag book

**Scene** says who may be in the scene: **adults** means only adults are in it; **any** means the gag carries no innuendo and shows no rear, so it may play in a scene with a child (rule 3). **First use** is the episode it lands in first; **bank** means written but unplaced, for the room to spend.

**Appetite (the feast gags)**

| Gag | Timing at 24 fps | Scene | First use |
|---|---|---|---|
| First seen mid-meal on a mountain of Brimstone Bagels, rear to camera, eating with enormous effort (two-handed, cheeks puffed, a groan of pleasure on every swallow) | The crossing's brick wipe lands on the rear {r} | adults | 102 cold open; reprised 113 |
| **The burped "1":** a burp; Sunny's pink "1" pops out, spins, lands on his tongue, and he swallows it again: "Mm. {..}Tastes like somebody new." | Spin 6 f; GULP 4 f after it lands | adults | 102 cold open |
| "I only had a light snack—" [R5: 4 f] and a forklift reverse-beeps in with the next course [DROP] | Beep on the line's cut-off; the DROP 4 f later | any | 102 cold open (on the bagel mountain); reprised 113 |
| Rating a dish: "Six. Needs salt." | Flat, after a 6 f chew | any | 101 (only as GURGLE's caption, "...needs salt."); 102 in his own voice (the Small Puff) |
| **The snatch:** a basket vanishes through the grille; CHOMP; GURGLE [R8]; "{d}...Seven." | Snatch on 1s (a 2 f smear); CHOMP 4 f later | any | 102 (Act One out) |
| "...Is there dessert?" [R8: 6 f] GURGLE: gurgle. [R13: 10 f] | | any | S3 |
| The buffet empties around him, bigger every time the camera "accidentally" cuts back [3x@12] | Three cutbacks 12 f apart, then a topper | adults | S3 festival |
| "Light snack." in a too-tight shirt, carrying the forklift's next course up the stairs | | any | S3 |
| The line-cook offer: "You'd have to wear a shirt." "...A TIGHT shirt?" {r} | Record scratch on "TIGHT" | adults | S3 |
| **Innuendo on every cooking verb** at Brimstone Bites: stuffing, basting, "a good long simmer", "you have to let it rest after", "I like a firm bun" | One verb per line, never two | adults | S3 |
| **Gas both ways:** a burp, then a fart, each a tempo-synced chiptune raspberry with a vinyl-crackle tail; the X pops on the fart's shudder; a cute green puff with hearts in it under the filter, sulphur-yellow with flies in the truth. He wafts it toward whoever is nearest: "{d}Compliments to the chef." | Burp; R8; fart; the line at R13 | adults | 102 (Act Two's afternoon) |
| **The smoke-ring burp:** three perfect rings drift out through the grille, each bigger; he hooks a claw through the third. "{d}Showing off." | [3x@12], topper 10 f later | adults | bank (a 103-106 grille beat) |
| **A gross-out close-up** (2-3 an episode at most): his face mid-burp with a wobbling uvula and bits of bagel; a string of drool that will not break; the belly skin stretching with a stitch seam about to go | Smash cut in, HOLD 13 (10 f), {r} out | adults | from 102 |
| Asking about leftovers; there are none; he asks again | | any | The special |

**The belly**

| Gag | Timing at 24 fps | Scene | First use |
|---|---|---|---|
| Gut sucked in [HOLD 13: 10 f], springs out [JELLY] {r}, and a harness stud pings off and hits something (*tink*): the forklift driver's hard hat, the Mayor's top hat, the pet | Stud flight 6 f, *tink* on contact | any | 102 (at the Mayor); 113 |
| **Unbuckles his belt with a dramatic sigh** [R21: 17 f] (canon: "The belt comes off.") | The buckle's *clunk* on the peak of the sigh | adults | 113 |
| **The two-belly conversation:** the pet beeps; GURGLE gurgles back; they keep going while everyone else waits | Alternating 12 f apart, four exchanges | any | 102 |
| GURGLE translations: "He says hello." / "He says you're cute. {..}He's not wrong." / "The belly says yes. The belly outranks me." / "He says go on, kid." (to a kid, never in a flirt) | | "cute": adults; the rest: any | 102, 113, S3 |
| **GURGLE contradicts him:** "I'm not blushing. {..}It's the pit." GURGLE: gurgle. "{d}...He says I'm blushing." | The contradiction R8 after the lie | adults | 102 (the blush) |
| The belly X-ray: a 4-frame insert, "60% air, 40% bagel". The truth's version: "60% air, 30% bagel, 10% Puff Demon (still waving)" | Insert 4 f; caption at R8 | any (the truth's version: adults) | S3 |
| The belly-flop: hangs in the air 2 f, `SHK_ROAR` 11 f, a local flash | | any (action) | 113 |
| The belly trampoline in the fight [SQUASH 1.3/0.7]; **burp portals** in threes [3x@12] (canon; they burp imps into the arena) | | adults (action) | S3 |
| The belly-bump on the last stubborn section of the Wall [SQUASH 1.4/0.7] | Part of the Wall-falls action, played straight: never a joke there | any | Series finale |

**The butt (bare, shown off, every reveal scratched; adults-only scenes, rule 3)**

| Gag | Timing at 24 fps | First use |
|---|---|---|
| **The grille opens rear first:** the whole little window is butt {r}. It turns: two hooded eyes, a fang. "Well, well. {..}Somebody knocked." | Shutter *clack* on frame 0; {r} on the first frame the window reads as butt; HOLD 21 before he turns; the turn over 6 f | 102 |
| **He bends for a dropped bagel in front of the window** (restored; full timing in §7.8): he drops it {r}, bends slowly and fills the window, stays down there eating it off the floor while the scene behind waits, the X pops in (*tink*), Nana's R8, then "Evening, sugar. {..}Nice buns." | The drop, {r}, a 17 f bend, HOLD 21 down there, the X held R3 | 102, at the grille (restored); reprised in the church window, S2 (canon Ep 3.1; the kids' BEEP plays as its own scene) |
| Presses it to the gate: "Feel that, padre? {..}Ten years of devotion." | On the Split (§8.2); the Gate creaks, jealous, R8 after Fuzz's "No." | 102 |
| **The butt-print signature:** every document handed to him comes back signed with a big butt-print where the signature goes | SLAM, stamp table (§4.1), with a wet *plap* and {r} | 102 (the noise complaint); every document after; S3: Signhere files one ("This is in breach of nothing. I don't know what to do with that.") |
| Slides down the bagel mountain on it [BOING]; struts away camera-side, the rump a beat behind | The strut on 2s, the rump R3 late | 102 cold open |
| Backs butt-first into the gatehouse and hip-checks the door shut | Door slam `SHK_POW` | 102 cold open |
| Admires it in a lava puddle: a slow look over his shoulder, a satisfied "Mm." | The over-the-shoulder hold runs R21 (17 f): too long, on purpose | 102 |
| **The Board's covers fail** (§6.20): a doily over the grille (eaten); the cupcake balloon (popped with a fang; it whistles off over the Wall); the GIGGLE banner (licked: "Strawberry."). Topper: the Mayor paints the grille pink, and from inside a tongue licks a clean peephole | Rule of three, about 12 s a cover; the topper R13 after the third | 102 |
| The blush on both sets of cheeks, glowing pink through the bars | The face blushes first, the seat 4 f later | 102 |
| The X pucker on the shudder (the front-fart beat) | Pops in on 1s, held R3 | 102 |
| Squeezes through the cracked gate rear first and sticks [HOLD 13: 10 f]: "{d}Tight. {..}Ten years and it's still tight."; BOING, he pops through [SQUASH 1.4/0.7] {r} | | 113 |
| Walks past a window backwards, eating {r} | Scratch on the first frame he's in shot | S2 |
| Backs into a doorway; the camera tries left, tries right, can't get round him [3x@12] | The camera is the victim | S2 (the Spire door) |
| **The wiggle taunt:** a 3-frame telegraph before the sit-slam | 3 f, then the slam | S3 (the fight) |
| Sits on things: the Big Chair (three citizens to a cushion); the throne, which says "{d}it's fine" and collapses. On people only by invitation: "Mind if I sit?" "...Go on." GURGLE: gurgle. | | S3 |
| **The cheek drum:** in "Open Late" he plays his own rump on the theme's six-hit stab | On the six hits | S3 (the number) |
| **Tanning on the gatehouse roof**, rump to the sky: "{d}Both of me deserve it." | | bank |

**Vanity**

| Gag | Scene | First use |
|---|---|---|
| Admires himself in anything shiny: a lava puddle, the grille's brass plate, a spoon, the Mayor's shoes | adults | 102 (the lava puddle); S3 |
| Rates himself: "Eleven. {..}Needs nothing." | adults | bank |
| **The rolodex on a forklift** (the little black book, upgraded): the driver reverses it up to him; he spins it, and it never stops on a name | adults | 102 |
| **The dating profile**, pinned to the gatehouse noticeboard (a 2 s freeze-frame gag): "Gatekeeper. Open late. Hung like a drawbridge (it's a metaphor, mostly)." | adults | 102 (Act Two's afternoon) |
| **The exes** (Shriekra, 587, a shriek vocalist; Spearmint, 591, a gate guard), voices only until 108. Shriekra, off-screen, at full shriek: "AND I WANT MY CHAINS BACK!" Spearmint, off-screen on his side, minty: "{d}You still owe me a spear polish." | adults | 102 (Shriekra); bank (Spearmint, a 103-106 grille beat) |
| "You're cute when you're scared." To his own reflection, or to Sunny (D13): "Well, hello, bunny. {..}You're cute when you're scared." Sunny's paw goes up. "{d}Noted." | adults | S3 (the Underbelly) |
| "Mayor. Love the hat." "It's my head." "I know." | adults | S3 |
| "Sugar, I was born this way." (his answer to every complaint) | adults | bank |
| The dating news ticker: "GRUMBLEGUTS DUMPED BY GATE: 'IT'S NOT YOU, IT'S YOUR MAINTENANCE SCHEDULE'" | adults | S3; Park Life |

**Flirting and raunch (adults only)**

| Gag | Scene rule | First use |
|---|---|---|
| To Sunny, through the grille: "He says you're cute. {..}He's not wrong." Sunny's ears go flat; a slow headshake. "Shy. {..}Noted." (one step back from the bars) | Sunny per D13 | 102 |
| "Mayor. {..}You look like trouble. Lucky me." MAYOR: "I am the MAYOR." "I know. I've seen the posters. {..}I keep one by the bed." | The Mayor retreats on "Hee. Hee." | 102 |
| "Feel that, padre? {..}Ten years of devotion." FUZZ: "{d}No." [R8] "Playing hard to get. I respect it." (one step back on "I respect it.") FUZZ: "{d}I'm not playing." [R5] "...I respect that more." (a second step back) | The Split; Fuzz's button is never cut (rule 4) | 102 |
| "Evening, sugar. {..}Nice buns." NANA: "They're MUFFINS." [R8] "I'll allow it." NANA: "{d}Sweet pea. {..}I've seen bigger." | After the bend (§7.8) | 102 |
| "Take a seat. Take two. I'm sturdy." The forklift driver reverses away, beeping | | 102 |
| **SUITE CLOSED:** an adult visitor's voice from inside: "You said 'open late.'" "I did say that." The door shuts; the sign flips; the gatehouse rocks; a stud pings off the roof onto the forklift driver's hard hat: *tink*. Later, a satisfied voice leaving: "Same time Thursday?" / "Call me." "I won't." | Off-screen and implied; the visitor is an unseen adult who came to him (rule 5) | 102 cold open and Act Two |
| The riding crop: "Is that a riding crop?" "It's a conversation starter." | | 102 (the Mayor) |
| "Come up and see my gate sometime. {..}It's very big. {..}And hinged." | Aimed at the adults at the gate | S2-S3 |
| To the Gate: "Ready, darlin'?" [R8: 6 f] The Gate creaks back in harmony (its yes), then "I'll be gentle." Canon's "Don't be like that." is cut: the Gate is his crush, and its yes comes first (hard limit 2) | Vapula and Grumbleguts only. No alternate | 113 |
| **The bank** (`GRUMBLEGUTS_TONE.md`), at most one bank line a scene so each stays a joke: "I don't bite. {..}Unless it's requested." / "Say my name again. {..}Slower." / "Is it hot in here, or is that just the pit?" / "I keep my gate unlocked for you." / "Is that a Brimstone Bagel in your pocket, or are you just happy to be exorcised?" / "Always date a demon. {..}They're used to the screaming." / "Dying's just the world's worst hangover. {..}I'd know." | Any adult scene | bank |
| **The body-language set:** the slow once-over of whoever walks in; the lip-bite; a finger crooked in the harness strap; hips that arrive before he does; the belly pat that turns into a stretch; the back-arch against the gate; a chain hooked round his own wrist and tugged; the over-the-shoulder glance that lasts R21 too long; a wink on every second sentence; leaning far too close and **stepping back the moment they flinch** (rule 4, played as the joke on him). Reactions: bystanders fan themselves, Father Fuzz covers his eyes and prays, the Gate hums | The step back is never cut for time | from 102 |
| Flirts with Honkers' napkin; the napkin is not interested | Any adult scene | S3 festival |
| **His only crush is the Gate.** It creaks when he flirts with anyone else (jealous) and creaks along when he sings to it (harmony) | Running | from 102 |
| **Gatekeep** (404, she, a robot: "Has never opened the gate. Polishes it."), optional: she polishes the Hillvile face while he flirts at her through the grille. "{d}Not interested." The Gate creaks, jealous: a triangle | `PILOT_ROADMAP.md` D18 | 102 (optional) |

**The grumpy uncle (kids)**: no innuendo anywhere in the scene, his rear out of frame, no swearing (rule 3).

| Gag | Rule | First use |
|---|---|---|
| PIP: "Patrol. You have my friend's one." "{d}Ate it." "...That's LOST PROPERTY." "It's found property now. {..}It's in here somewhere." GURGLE. | Pip at the grille; his face in the window and nothing else | 102 |
| "Go home. Eat something. {d}Not THAT." | The kid is visibly reaching for food or a lava rock | 102 (Pip, a lava rock) |
| Slides his own bagel under the gate for the walk home: "{d}Didn't happen." | | 102 |
| To Bolt: "Eat your vegetables. {d}Not those. Those are mine." | | S3 |
| Volunteers to be the monster under the nap mat and falls asleep first; asks if there's a snack after nap time (there is; Lulla planned for it) | | Park Life |

**The antihero turn and the heart (played straight)**

| Beat | Rule | First use |
|---|---|---|
| **The Small Puff save:** his arm shoots out through the grille and drags the demon through the bars; on his side (the truth) he bites it in half, blood to the elbows. "Six. {..}Needs salt." Back at the grille: "Didn't do that for you." [R8] "{d}Did it for the snack." | The save is played straight; the denial comes after. Sunny and the pet only | 102 |
| "...Ten years. {..}Nobody knocks." | **Protected hold, 3 s, no button** | 102 |
| Sings the bridge of "Open Late" to the Gate when he thinks nobody is listening ("somebody has to keep the light on"); the Gate creaks along in harmony | | 102 |
| At the brick, he holds Charlotte's tin while she waits, and drops the act | No flirt in the scene | 112B |
| Drops the act to save his friends, then: "I totally didn't do that for you." | The save is never undercut; the line comes after | S3 |
| Holds the gate for the Mayor | | S3 |
| "There's a THING down there. It beeps. {..}It ate my lunch. {d}Nobody eats my lunch." | | S3 |
| "Stay behind me, friends." (the fight's last bark) | | S3 |
| "Kept this gate ten years. Nobody came up to see it." [HOLD 21: 17 f] | **No button.** Pays 102's "Nobody knocks." | S3 |

### 7.5 The ladder across the series

| When | What the audience gets |
|---|---|
| **101** (not seen) | The main title's chorus: behind the gate's shut grille, a big round rear silhouette presses against the gate and bounces once, BOING; the padlocks jump. The grille, its shutter shut, is in frame in MT-4, 4-03 and PC-1 (the plant). Post-credits: a wet crunch, the gurgle, three padlock rattles, BOING, GURGLE's caption "...needs salt.", then his purr |
| **102 "Open Late"** | The debut (`SERIES_OVERVIEW.md` §5.7; the gag map is §7.8): the camera crosses to his side for the cold open; Sunny knocks; the grille opens rear first; he flirts with every adult and gets a no from each; the restored bagel bend; the Board's failed covers; the butt-print signature; the grumpy uncle with Pip; the save; "Nobody knocks."; the blush |
| **The main title from 102** | He pops into the verse rear first with a record scratch, the only cast member who enters backwards, and winks over his shoulder on the next beat, in a verse shot of his own with no child in it (Pip pops in on another; rule 3) |
| **103-107** | The grille is a stop on Sunny's week, one beat an episode, kid-free unless he is the grumpy uncle: 103 the late-show heckle; 104 "gift from G" (a Gate Key (Copy)); 105 the grape smell reaches the grille (optionally, Vex at the grille); 106 a card on Bloom's board; 107 the grille stays shut all episode, and nobody comments |
| **108-112** | He lives in Hellvile, so the B-segments get one Grumbleguts cameo each, in a kid-free scene (Anvil's hinge, Charlotte's cookie, Sutura's clinic after hours, the Thursday brick), plus the distant GURGLE that rattles the cups |
| **113** | The turn: Vapula's promised feast; "I'll be gentle."; the belly-flop; the crack; the rear-first squeeze into Hillvile; he does not wink at Sunny. Post-credits: "...Five hundred and one." |
| **Season 2** | In Hillvile: the church-window reprise of the bagel bend (canon Ep 3.1, split from the kids' BEEP scene, which plays as its own scene); he eats the Fun Fair's cotton-candy machine, machine included; he blocks the Spire door |
| **Season 3** | Canon: the toll ("Nobody passes without giving me their number."; a cupcake: "The belly says yes. The belly outranks me."); the Underbelly fight and the dish-feeding pacifist route; "...Is there dessert?"; the line cook ("...A TIGHT shirt?"); "Mayor. Love the hat."; "Kept this gate ten years. Nobody came up to see it." (pays 102); the "Open Late" number; the antihero saves; the buffet; **the festival's button: his belt pings off and hits the Mayor's hat** {r} (canon); the credits: he waves a drumstick at the camera, then at the Gate |
| **The Feast special** | "The Feast of Rebuilding" (title card: "What an Ass"), after Season 3, in its consent rewrite (`SERIES_OVERVIEW.md` §6.1) |
| **Park Life** | Feed the Gate as a cooking-and-friendship story; the gatehouse redecorated (S185); a little heart painted on the repaired Gate. Nobody knows who. Everybody knows |

### 7.6 Never

1. **Anything past the four hard limits**, in any episode, cut, promo, short, deck page or product.
2. **The S186 Feast outcome as the game writes it.** There the hose goes into him without his say-so, which hard limit 2 rules out, and the crowd's "...that's hot" lands on that act. As written it stays game-only (`PILOT_ROADMAP.md` D17). The show's version is the post-Season 3 special in its **consent rewrite** (`SERIES_OVERVIEW.md` §6.1 and D10 there): the hose is the feast's soda line and he asks for it, the push and the slap are asked for, and the joke is his gluttony.
3. **Copying another show.** The comps are tonal only: no borrowed designs, names or lines. Angel Dust is a type, not a template.
4. **The third-party references** in `docs/design/story/refs/*.webp`: inspiration only, never in show or pitch material.

**Back in from the old never list:** the `GRUMBLEGUTS_TONE.md` passes (the propositions, the adult humour and the swearing, the gag bank and the dating bio, the very suggestive body language, the gratuitous and distasteful passes, the front-fart shot with the X), the harness and the studded leather, the riding crop and "Is that a riding crop?", "Darlin'" and "sugar", "The belt comes off.", gas both ways, the unbuckle and flirt poses, the gross-out close-ups (rationed by §7.1 rule 7), and "I'll be gentle." with no alternate (after the Gate's yes).

### 7.7 Before any Grumbleguts scene is boarded

1. Any child or teen in the scene? Then grumpy uncle only: no innuendo anywhere in the scene, his rear out of frame, no swearing, and no graphic gore in any frame with them.
2. Who is he aiming at? An adult, the Gate, an object or his reflection (Sunny per D13, with a visible no).
3. Is every advance consensual, and does a no end it with a step back on his next line?
4. Any hookup on screen? Move it off-screen (the door, the sign, the rocking gatehouse).
5. Anything explicit (an act, genitals)? Cut it. The bare butt and the X are fine.
6. Is the joke his vanity, appetite or shamelessness, and not the target's body?
7. Every rear reveal scratched? Gross-out close-ups within 2-3 for the episode? One f-bomb at most?
8. Any censor sticker? Replace it with staging (§6.20).
9. Captions written ([GURGLE: gurgle.], [low purr], [record scratch]); standards note on the board.

### 7.8 His debut, gag by gag: 102 "Open Late"

Timings at 24 fps (Appendix A converts them to 30). The episode's outline is `SERIES_OVERVIEW.md` §5.7.

**The crossing** (cold open). The series' first full brick wipe across the Wall lands on the gate's other face: a buzzing sign, OPEN LATE (its flicker 2 f off, 21 f on, never faster). Tilt up a mountain of Brimstone Bagels, 13 f of smoothstep, to the rear on top {r}. Hold 10 f. The first sound after the scratch's silence is small (§4.8): one wet chew. Then the light-snack line and the forklift's DROP, then the burp, and the "1" spinning onto his tongue 6 f later.

**The grille opens rear first** (Act One, beat 6).

| Frame | Event |
|---|---|
| 0 | Sunny's third knock (the knocks run 12 f apart, each softer). Then HOLD 21 (17 f) of silence: every hedge holds its breath |
| +17 | The shutter *clacks* open. The whole window is butt |
| +18 | {r}: tape-stop over 4 f, then 17 f of silence |
| +39 | "bep?" (the pet: the small sting) |
| +45 | HOLD 21: the butt considers the question |
| +62 | It turns over 6 f: two hooded eyes and a fang fill the window |
| +74 | The crop taps the bars twice, 4 f apart: "Well, well. {..}Somebody knocked." |

**The bagel bend** (Act Two, beat 4). Nana's POV through the bars.

| Frame | Event |
|---|---|
| 0 | He stands right in front of the grille, back turned, finishing a bagel. Nana's face in the foreground, the second basket on her arm |
| +10 | The bagel slips: *plip* |
| +16 | {r} at R8: tape-stop over 4 f; silence |
| +20 | The bend: 17 f (R21), eased in, because slow is the joke. The rump rises until the whole window is butt at +37 |
| +37 | HOLD 21 (17 f): he eats the bagel off the floor, down there, chewing on 2s, with tiny wet *nom*s. The scene behind waits: a bird lands on the hedge, looks, and leaves |
| +54 | The X pucker pops in: *tink*; held R3 (3 f) |
| +57 | Nana's look: exactly R8 (6 f), no line (§6.10) |
| +63 | He straightens over 10 f and turns over 6 f; his face fills the grille at +79 |
| +85 | "Evening, sugar. {..}Nice buns." NANA: "They're MUFFINS." [R8] "I'll allow it." |
| | NANA: "{d}Sweet pea. {..}I've seen bigger." [R13] GURGLE {r}. She hangs the muffin on the nail anyway (two of everything: one is his now) |

**The Board's covers** (Act Two, beat 2). Each cover goes up on a SLAM, holds R21, and fails on 1s; the Mayor's "Hee. Hee." buttons the first two, and the third has no button so the topper can land. Doily: a CHOMP, a lace thread left on the bars. Cupcake balloon: one fang, a squeal, and it whistles off over the Wall on the flee curve. GIGGLE banner: one long lick, then "{d}Strawberry." Topper, R13 later: the Mayor paints the grille solid pink. Silence. Three wet squeaks, 6 f apart, from inside: a tongue licks a clean peephole, and an eye in it winks.

**The butt-print** (Act Two out). The complaint slides back out through the bars face down. The Mayor turns it over: a big butt-print where the signature goes, SLAM {r}. He holds it at arm's length for R21. "Resolution:" is still blank.

**The save** (Act Three, beat 2). On the Hillvile side the Small Puff goes through the bars in a puff of confetti (the filter). The brick wipe to his side is the truth: one bite, `SHK_IMPACT`, the top half of the demon in his mouth and the bottom half in his fist, blood to the elbows, a slow chew on 2s. "Six. {..}Needs salt." A tooth from the demon *tink*s off the gate. Wipe back: a single heart-shaped confetti piece drifts down past the grille. "Didn't do that for you." [R8] "{d}Did it for the snack."

**The blush** (Act Three, beat 4). Sunny's paw on the gate. His face blushes first; 4 f later the seat's blush glows pink through the bars. "I'm not blushing. {..}It's the pit." GURGLE: gurgle. "{d}...He says I'm blushing." (the soft button after the protected hold; the hold itself has none)

---

## 8. Visual gag rules

### 8.1 World physics

* **Plush wobbles, hardware is stiff, hell drips and never wobbles, Grumbleguts bounces.** No exceptions; a model sheet that breaks this is redrawn.
* **Nothing is ever static** (§4.18). A stopped clock is always a choice.
* **No cute citizen is ever gored.** Violence is for demons, monsters and hell-stuff; citizens get slapstick.
* **Two gore sets.** **The filter's** (HaHaland's): hearts, confetti, plush stuffing, grape goo (grape purple). **The truth's** (DOOM's): blood from the BLOOD ramp, gibs, viscera, eyeballs, teeth. The filter is what HaHaland sees. The truth shows in four places: the brutal register (§4.15); truth inserts (§4.16); Hellvile's side of the Wall, which has no filter (from 102, Grumbleguts' side of the gate); and the main title's chorus. **The gag is the switch**, and the switch always has a sound: SCRATCH in, REWIND out, with one persistent change.
* **The filter is always on when a child is in frame** (hard limit 3). Truth inserts are cut into frames with no child in them.
* **Gore lands on a beat and gets a button** (§4.15).
* **The HELL, BLOOD and LAVA ramps** appear only where the truth shows (and in the IN cards' flashes in the arc 1 episodes); from 113's eruption the sky may use them.

### 8.2 The Wall grammar

* **The camera can cross the Wall. Nobody else can** (until canon says so). Hands and things pass through the loose brick and, from 102, the gate's grille; nobody crosses until 113.
* **Screen geography:** whenever the Wall is face-on, Hillvile is screen-left and Hellvile screen-right.
* **The brick wipe:** bricks stack in 5 courses, 2 frames each, along a vertical line sweeping left to right (10 frames) going into Hellvile; right to left coming back. A trowel *tink* on the last course. The course count is a drawn step count, like the crayon line boil, so it stays 5 x 2 frames at 30 fps too (the pilot's beat 4-08 uses exactly that).
* **The Split:** the wipe stops halfway and holds, at least 2 s (48 frames), with the Wall line exactly at the frame's centre. Both halves keep their own sound. The left half often reuses the A-segment's footage.
* **The Split is for crossings only:** a name, a knock, a hand at the brick, a sign flipping on both sides. Never for a gag that could play in one shot.
* **Season 1's Hellvile is shot tight** (`SERIES_OVERVIEW.md` §4.7). From 102 that includes the gate's Hellvile face, the gatehouse and its keeper; other Hellvile people are hands, voices and silhouettes until 108 (and Tallyworn's faceless back in 107's coda).

### 8.3 One persistent change

After any insert, freeze or wipe-back, the shot returns **exactly** as it was with **one** detail changed: the balloon faces the Wall, a heart is still an eye, the lamp grew horns, the crack stays. Never two changes. The audience should be able to point at it.

### 8.4 Signs, props and diegetic text

* **The sign is the joke:** "NOTHING INSIDE! (we checked) —Mayor"; "TOP PRIZE. Please don't. —R"; "BAKE TEN"; "KEEP OUT. Underneath, smaller: FOREVER."
* Give every sign gag its reading time (§4.13) and keep the frame still around it.
* **Gifts stay tagged "gift from X" forever**, in every shot of the house.
* **Continuity props** (checked on every background): the Population sign reads 500; the gate has seven padlocks and a grille (shut in 101; open, painted pink and licked clean from 102); the fountain's two faces are turned away from each other; the crack stays after 113.

### 8.5 Background gags

* **Rate:** one per 20-40 s of walk-and-talk.
* **Never** during a punchline's line, a protected heart hold, or a horror hold. Exception: the one wrongness a horror beat is built on.
* **A wrongness is noticed once and never explained** until its canon reveal (the flowers leaning toward the Wall, the lamp post that faces the Wall and then faces you, the shadow two frames late, the bell that un-rings).

### 8.6 Rewatch clues and freeze-frame gags

* The show rewards pausing: the painted-over 1000, the pipe's purple sigh, the handwriting on the old draft, the cupcake on the grave.
* **Every hidden image is on screen for 4 frames or more.** No single-frame cameos, ever (subliminal-image rules).
* **A clue is fair:** a first-time viewer can see it if they look, and can't act on it until its episode.

### 8.7 The registers

| Register | When | Rules |
|---|---|---|
| **Crayon** | History: the cold opens, the end credits, memory | Children's-book crayon on paper grain, a 2-frame line boil, captions in a child's hand, no faces for kid Giggs and kid Nox |
| **The crayon bleed** | The Mayor remembering | Crayon bleeds in at the edges of his shot for R8 (6 frames) |
| **Sepia** | Flashbacks (F1-F4) | A palette lerp in and out, the REWIND sound on the way in |
| **LCD** | The pet's dreams, its inserts, the HUD face | LCD green, chunky pixels, piezo sound |
| **Hellify** | The DOOM switch | The palette rots pixel by pixel over 17 frames on the way in; REWIND back to cute, with one persistent change |
| **The truth insert** | Any filtered moment, 4 frames | The same frame as it really is (§4.16); owns the audio; returns with one change; never in a frame with a child |
| **The HUD** | First-person runs only | Hearts on the left, shells on the right, the pet's face in the centre, the weapon plate (it flips with the register); our own layout, no third-party assets |

### 8.8 Kids on screen

* We laugh **with** Pip, never at Pip's fear.
* Kids are brave in the show: Pip says the name, Pip patrols the Wall.
* **Hard limit 3:** child and teen characters are never in sexual or suggestive scenes or dialogue, and never the subject of graphic gore. A scene with a child in it carries no innuendo at all, and the filter stays on in every frame with a child in it. **No child is ever possessed or touched by the fog** (canon).
* Kids can be in a rite's room only behind an adult, and never within reach of the possessed.
* Kid peril can be real; graphic gore never is (the filter stays on).

### 8.9 Who the joke is on

* **Hellvile is never the punchline for being Hellvile.** The gag is always Hillvile's avoidance, or a Hellvile character's own quirk, never "they're from over there".
* **Faith is affectionate:** Father Fuzz's rite, the hymn and the water are never mocked; "THE POWER OF CUDDLES COMPELS YOU!" is a parody of a film line, not of a church.
* **No accent gags.** Steamy's lilt is a performance, not a punchline.
* **Vaping is the villain's marketing,** and the joke is on the marketer ("That's the business model.").
* **No real-world hate content** (hard limit 4): no slurs, no real-world hate symbols, no joke whose target is a real group, faith or person.

### 8.10 Pixel discipline for gags

* Every gag frame passes the palette lint: smears and flashes included.
* Integer camera moves and integer scaling only.
* No frame blending, no motion blur, no anti-aliased smears.
* Particles (confetti, hearts, gibs, blood droplets) are palette sprites, never alpha.

---

## 9. Sound gags

* **Every cut has a sound on frame 0.** The only exception is the season's one silent smash (§4.7).
* **One sound per visual beat,** frame-locked. Reactions get tiny sounds: a "tik" for eyes, a "plink" for an asterisk, a "shwip" for an iris, a gulp, a "huh?".
* **Duck under the joke's little sound** (to 0.4-0.75 for 5-20 frames): under "beep.", under a "!", under Fuzz yelling.
* **The record scratch protocol** (§4.8): scratch, silence, a small sting, then a big one if needed.
* **The kazoo of failure:** one note per fail, each lower; three notes on the third.
* **The Hee-Hee bell** opens every act card and every OUT eyecatch.
* **The ITS_FINE sting** resolves every IN eyecatch.
* **Theme-derived stings** (confirm against the master in `docs/show/theme/`):
  * the bus horn is the hook's first two notes;
  * the eyecatch OUT is a 2-bar cut of the hook in major, the IN the same bar in minor;
  * if the revised master keeps the demo's six-hit stab rhythm in its intro, it becomes the hit pattern for kime moments (a cast pose landing on six hits);
  * if it keeps the demo's chromatic "hell slide", it scores the hellify switch.
* **The switch has its own sounds:** SCRATCH into the truth, REWIND out of it; the Pump Shotgun *squeaks* in the filter and goes CHK-CHK in the truth; the saw purrs, then screams. A truth insert brings its own wet sound and owns the audio for its 4 frames.
* **The leitmotifs are gags too:** HaHaland's tune and Hellvile's are the same melody in major and minor; **a possession is the moment the fourth note bends** from C to B♭.
* **The babble rhythm for deadpan:** low syllables 5, 3, 2, 2 frames apart; a period holds 5 frames.
* **Silence budget:** §5.3.

---

## 10. Accessibility, captions and localisation of gags

### 10.1 Captions

* Comic sounds that carry a joke are captioned: **[beep.]**, **[GURGLE: gurgle.]**, [low purr], [record scratch], [squeak], [*tink*], [kazoo, lower], [the fountain giggles, late], [wet crunch], [the gatehouse creaks rhythmically].
* Captions sit clear of burned-in diegetic text and never cover a sign gag.
* A caption for a 267 ms-late sound appears on the late beat, not the expected one.

### 10.2 Audio description

Mandatory, because the lead is silent.

* Describe every Sunny action (looks, points, carries, pours, picks the card).
* Describe sight gags **in the hold after them**, never over the punch line, never over a protected heart hold.
* Read every sign that is the joke, in the same deadpan register as the show.
* Never describe a rewatch clue as a clue; describe what is there.

### 10.3 Localisation

| Item | Rule |
|---|---|
| **"beep."** | Never translated. Same lowercase, same period, in every language. |
| **Pronouns** | They (Sunny, Pip, Zoomzoom, Hauntessa, Drudge, Ploddy), xe (Bolt, Sootkin, Sutura), it (the pet, GURGLE). The per-language guide defines each language's form; no character is re-gendered to fit a grammar. |
| **GIGGLE over MINGLE; BAKE TENT to BAKE TEN** | Translators may rebuild the wordplay with the same rule (one painted-over word that bleeds through; one fallen letter that makes a new word). |
| **"Nice buns." / "They're MUFFINS."** | Keep a baked-goods double meaning at the same strength: it is innuendo, and should stay innuendo. |
| **"Open late." / SUITE CLOSED** | Keep both meanings, the shop sign and the bedroom. |
| **Swearing** | Translate at the same strength; never substitute a slur. |
| **The weapon plates** | PARTY PUMP / SLAUGHTER and PURRING SAW / MEATCLEAVER: a cute name over a brutal one, in the language's own register. |
| **Knock-knock jokes** | Use the language's own call-and-response form. |
| **The ratings** | Numbers out of ten stay numbers; "Needs X." keeps its flat shape. |
| **"Hee. Hee."** | Keep two syllables with a stop after each. |
| **The theme line** | Translated once, consistently, and used only in its three canon places. |

---

## 11. Checklists

### 11.1 Per scene (from the timing contract, `story_final.md` §7)

- [ ] At least one camera move or cut
- [ ] One expression change per line of dialogue
- [ ] One sound on every visual beat; a sound on frame 0 of every cut
- [ ] One silence (MUTE, SILENCE or TAPESTOP)
- [ ] One button, or an explicit `[NO BUTTON]`
- [ ] Gaps on the ladder (§2.1), within a frame
- [ ] Entrances in 4-5 frames with overshoot; squash anchored at the feet
- [ ] Inserts exactly 4 frames, owning the audio, returning with one change
- [ ] Rule of three at a constant 12-13 frame interval, topper 10 frames later
- [ ] At most one deadpan in the conversation
- [ ] Density inside §5.1 for the scene type

### 11.2 Gag standards

- [ ] **The four hard limits:** nothing explicit; adults and consent; no child in suggestive material or near graphic gore; no real-world hate content
- [ ] Gore on demons and monsters only; the filter and the truth assigned correctly; the filter on in every frame with a child; every gore beat buttoned
- [ ] No censor sticker, unless it is the season's one sticker-as-joke (§6.20)
- [ ] No child possessed or touched by the fog
- [ ] Grumbleguts: §7.7 passed
- [ ] No flash shorter than its rule or brighter than its cap; no image under 4 frames; full-screen flashes 3 a second or fewer; blood never strobes; large red changes logged for Harding
- [ ] Vaping unglamorous; no real brands
- [ ] The joke is not on a kid's fear, on Hellvile for being Hellvile, on consent, on faith, on an accent or on a real person
- [ ] The theme line used only in its canon places, and never sung
- [ ] Sign gags localisable; translator note written for wordplay

### 11.3 Animatic review

- [ ] Every frame-critical (FC) hold intact and counted
- [ ] The laugh map passes (§5.2)
- [ ] The silence budget is 20-25% per act
- [ ] Protected holds intact (no music under them, no gag in them)
- [ ] Truth inserts intact at 4 frames, each returning with its one change, none in a frame with a child
- [ ] The final story beat of the episode has no button; its button is after the credits
- [ ] Running-gag continuity: the Population sign, the padlocks, the fountain faces, the little bed's cell, Mascotty's step in the IN cards
- [ ] Captions and audio description scripted for every sound gag and sight gag

---

## Appendix A: every number in this bible at 24 and 30 fps

| Item | Duration | 24 fps | 30 fps |
|---|---|---|---|
| R1 / R2 / R3 / R5 | 33 / 67 / 100 / 167 ms | 1 / 2 / 3 / 4 f | 1 / 2 / 3 / 5 f |
| R8 / R13 / R21 / R26 | 267 / 433 / 700 / 867 ms | 6 / 10 / 17 / 21 f | 8 / 13 / 21 / 26 f |
| Entrance settle | 133-208 ms | 4-5 f | 4-6 f |
| Insert | 133-167 ms | 4 f | 4-5 f |
| Rule-of-three interval | 500-533 ms | 12-13 f | 15-16 f |
| Button after the punch | 400-833 ms | 10-20 f | 12-25 f |
| Crowd ripple | 0-125 ms | 0-3 f | 0-3 f |
| Iris out / hold / in | | 4 / 3-4 (max 8) / 5 f | 5 / 3-5 (max 10) / 6-7 f |
| Record-scratch tape-stop | 160 ms | 4 f | 5 f |
| Scratch silence | 0.47-2.13 s | 11-51 f | 14-64 f |
| Freeze hang time | 467 ms | 11 f | 14 f |
| Covered cut | 733 ms | 18 f | 22 f |
| Hit-stop (pop, blast, kill, stagger, boss) | | 2, 2, 3, 4, 6 f | 2, 3, 4, 5, 8 f |
| Idle: breath / tail / blink / ears | 1.3 s / 0.75 s / every 2.8 s / every 3.2 s | 31 / 19 / every 67 / every 79 f | 39 / 22 / every 83 / every 97 f |
| Hop | 233-250 ms | 6 f | 7 f |
| Mascotty's slow wave phase | 250-267 ms | 6 f | 8 f |
| The fountain giggle and the "we" voice | **267 ms, in the mix** | lands on frame 6 | lands on frame 8 |
| Heart hold | 3-5 s | 72-120 f | 90-150 f |
| Camera creep | about 2 native px a second | 1 px every 12 f | 1 px every 15 f |
| Brick wipe (5 courses, a drawn step count) | 333-417 ms | 10 f | 10 f |
| The Split's minimum hold | 2 s | 48 f | 60 f |

## Appendix B: sources

| Topic | File |
|---|---|
| The story, the cast, Grumbleguts' canon and hard rules, the running gags table, the timing contract | `docs/design/story/story_final.md` §0-§7 |
| Timing measurements, techniques, density, accessibility | `docs/design/story/comic_timing.md` |
| Grumbleguts' tone, gags and romance (adopted within the hard limits; the S186 Feast as written stays game-only, §7.6) | `docs/design/story/GRUMBLEGUTS_TONE.md`, `GRUMBLEGUTS_AND_DIRECTOR_NOTES.md`, `GRUMBLEGUTS_ROMANCE.md` |
| The weapons' cute and brutal names | `core/src/weld.c` |
| Concept sheets | `docs/design/story/concept/*.png` |
| Shipped lines | `core/data/story/ch1/*.dsc`, `core/data/story/ch2/*.dsc` |
| Pronouns and species | `core/data/roster/*.txt`, `docs/ROSTER.md` |
| The trailer's springs, tables and sounds | `trailer/tk.c`, `trailer/tk_chibi.c`, `trailer/music.c`, `trailer/scenes/` |
| The theme | `docs/show/theme/` (in production) |
