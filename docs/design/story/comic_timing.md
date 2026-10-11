# Comic Timing Bible: DOOMOGACHI LIFE in HELLvile Park!

This document reverse-engineers the comic timing of our 78-second teaser
(`trailer/`) into rules, exact frame counts, and fixed-point math. Story,
dialogue, overworld, combat, pet and housing code can use it so that both
games have the trailer's spastic, elastic, deadpan rhythm.

* **Sources.** All numbers come from the trailer source. Every number below
  can be traced to `trailer/timeline.c`, `trailer/scenes/sNN_*.c`,
  `trailer/tk.c`, `trailer/tk_chibi.c` and `trailer/music.c` (file:line
  given where useful).
* **Frame check.** I also rendered the beats and looked at them: the trailer
  was rebuilt into the scratchpad from a copy, never in the repo. Contact
  strips are in `story/frames/*.png`.
* **Units.** 1 frame = 1 game tick = 1/30 s = 33.3 ms. Both games simulate at
  30 Hz, so trailer frame counts carry over 1:1.
* **Pixels.** Pixel amplitudes are in **GBA pixels** (240x160). On PS1
  (320x240), scale x by 4/3 and y by 3/2. That is `amp * plat_screen_w() / 240`
  and `amp * plat_screen_h() / 160`.
* **Fixed point.** Math follows the core's conventions (`core/include/doomo/types.h`):
  * `fix16` = 16.16.
  * Scales are Q8.8 (`256 = 1.0`), as in `ui_pop_scale()` and `ui_heart_squash(sx256, sy256)`.
  * Angles are 0..4095 (`fx_sin`).
  * Springs use Q4 positions with Q14 coefficients, so every multiply fits in 32 bits.

---

## 0. The 12 rules (summary)

1. **Every entrance is 4-6 frames, and it overshoots.** Appearing things pop
   from 0.15-0.4x to 1.12-1.35x. Slammed things land from 1.6-2.4x down to
   0.81-0.95x. Dropped things squash to 1.22-1.38 wide. All of them settle by
   frame 5-6 (section 3.1).
2. **Comic gaps sit on a ladder of 2, 3, 5, 8, 13 and 21 frames.** About
   three quarters of the 34 measured setup-to-reaction-to-caption gaps fall
   within one frame of these rungs (section 2).
3. **A reaction comes 2-3 frames after the event when it is instant, and
   5-8 frames after when it is dawning.** A deadpan reply comes 18-26 frames
   after.
4. **Silence is an instrument.** The score is muted for **539 of the 2340
   frames (23%)**. 13 of the 15 scenes carve out a silence for their punch;
   the other two (s09, s12) put a hit on it instead (section 4).
5. **Stillness is the scariest and funniest thing in a world that never stops
   moving.** The baseline idle is breathing, blinking, twitching and wagging
   on co-prime periods. Freezing an actor's clock is therefore the joke: the
   heart that stops breathing, the starer, the possessed bunny (3.7, 3.12).
6. **A tonal whiplash insert lasts 3-4 frames, never more.** Cut back to the
   cute shot with exactly one detail changed. A small sound comes 2 frames
   later, and the deadpan line 8 frames after that (3.16).
7. **The rule of three uses an identical interval with escalating
   magnitude.** In s11 the three failures come 15-16 frames apart. The
   third failure gets a longer shake, a red flash, a 1.2x larger counter
   and a three-note "wah wah wahhh". Then a topper comes 13 frames later
   (3.11).
8. **Deadpan means no bounce, no music and no idle animation.** The text is
   small or plain: it slides in without overshoot, or simply appears (3.12).
9. **Every joke gets a button.** A tiny after-beat lands 12-25 frames after
   the punch, often inside the 3-5 frame hold of an iris-out. Examples: the escaped
   drip, the lone blink, the slow wave, the balloon pop, the *tink* (3.18).
10. **Crowds react as a ripple, not a switch.** Stagger crowd reactions by
    0-3 frames using a hash of each citizen's id (3.9).
11. **The setup types and the punch pops.** Setup text reveals at about 24
    px per frame. Punch text slams or pops in whole. Deadpan text just
    appears (3.13).
12. **Shake has a profile, not a random number.** x alternates every
    frame, y every 2 frames. Amplitude goes 3-2-1 for taps and 7..1 for
    shotguns. Shakes last 2-14 frames (3.14).

---

## 1. Beat sheet: the anatomy of all 15 jokes

Frames are local to each scene, at 30 fps. The columns are:

* **SETUP**: the cute part.
* **ESCALATION**: intervals shrinking or magnitude growing.
* **PUNCH**: the frame where the joke turns.
* **REACT** and **TAG**: offsets in frames from the punch.
* **BUTTON**: the small after-beat.
* **EXIT**: how the scene leaves.

| # | scene (frames) | SETUP | ESCALATION | PUNCH | REACT | TAG / caption | BUTTON | EXIT |
|---|---|---|---|---|---|---|---|---|
| 1 | s01 splash (75) | heart falls 0-13, lands 14, two bounces 17-34, letters pop 17-31 | - | devil tail **44-46** (a 3-frame insert) | freeze 47 (+3); side-eye 50 (+6) | "\*" 52, "\*mostly" 55 (+11) | - | white fade 70-74 |
| 2 | s02 title (120) | title drops 2-22, LIFE slams 19, shine 40 | drip forms 44, eyes look down 50, nervous jitter 58-67 | fling 68 (6 frames), **SPLAT 74** | 10 frames of silence | sticker 84, "nothing to see here!" 87 | drip escapes 96, 2nd sticker 104, whistle 108 | heart iris 111-115, hold 116-118, black 119 |
| 3 | s03 HaHaland (150) | parallax pan, "Welcome", "Population: 500" | constant-speed pan (no ease-out) | **record-scratch brake 96** | red eyes 102 (+6), one every 3 frames, the last one 9 frames late | "(and 500 not-so-nice...)" 105 (+9) | a citizen waves 112, the cyclops waves back | cut |
| 4 | s04 Doomogachi (165) | drop, hatch 36, feed 58, love 78 | strain 92, poop 100 | **4 frames of hell 128-131** | 2 frames, then "beep." 134 | "...it's fine." 142 (+8 after the beep) | the horns stay | cut |
| 5 | s05 creator (150) | window boing, a new look every 4 frames | a new look every **3** frames from 58 | lands 100, click 104, **snap-zoom 109**, eye 113 | mouth "O" 115 | "Yes, three eyes." 118 | only the 3rd eye blinks, 145-147 | iris 139-143, hold 144-148 |
| 6 | s06 life (150) | high five 7 | anger 13, grawlix 15/19, lightning 22 | **POW 30** | - | list of three: friends / love / cupcakes | the starer: "Ignore that one." 114, slow wave 136 | iris 140-144, hold 145-148 |
| 7 | s07 vape (165) | grand opening, Vex's pitch 40-91 | inhale 95-104, held 105-109 | **FWOOMP 110** | the cloud covers the cut for 22 frames, red eyes in the fog 120 | "Everything is fine." 134 (+14 after the reveal) | creak 140/143, balloon pops 158 | cut |
| 8 | s08 invasion (180) | park party 0-13 | - | **RECORD SCRATCH + freeze 14** | "!" ripple 16-19, "beep." 21 (+7) | sky cracks 28 (+14) | crown lands crooked 84 | slam captions |
| 9 | s09 weld (135) | parts land at 8, 18, 27, 35, 43, 49 (gaps 10-9-8-8-6) | a spark hits the tail at about 41 | **sign 7 -> "-" -> 0, 46-48** | mask up 54 (+8) | thumbs-up fanfare 58 | the tail is still on fire | flash cut 70 |
| 10 | s10 shooter (255) | cute kill 24-31 | brutal kill 106 | **"(We don't judge.)" 138** | +26 frames | "(We judge a little.)" 164 | Father Fuzz +3, "tsk." +10 | whoosh |
| 11 | s11 exorcism (225) | "THE POWER OF CUDDLES" 8-59 | **FAIL x1 92, x2 107, x3 123** | wah-wah 124/130/136 | "SOUL LOST." 136 | the ghost reads its map upside down, 148+ | the cursor sneaks 178-185, MONSTER MODE 186 | cut |
| 12 | s12 census (120) | counter rolls 0-21 | 1000 / 500 / 500, every 22 frames | **1 PARK 90 -> cork 108** | - | "(cozy.)" 111 (+3) | the cork crowd-surfs | cut |
| 13 | s13 ending (150) | sad stare, rumble 16 | crumble 36 | hug + anthem 64 | - | "everyone can get along." | tilt 98, "(after a LOT of shooting.)" 110, **tink** 114/120/124/127 | cut |
| 14 | s14 platforms (135) | handheld 8, console 56 | the disc rises 64 | **"Yes, the actual PlayStation." 74**, music dies | +18 frames | "The grey one." 92 | arms 102, wind-up, clap 112 | cut |
| 15 | s15 logo (165) | logo drops 6-15 | ignite 30, shocked eyes for 14 frames | **looks at the camera 44** | +8 frames | "(it's fine.)" 52 | post-credits: pop 120, strain 127, plop 135, horns 144, beep 150 | fade 158-164 |

The shape is the same every time: **setup (40-60% of the shot), escalation
(shrinking intervals), punch (an event of 4 frames or less, plus a sound or
a silence), reaction (2-8 frames), tag (3-18 frames), button (12-25
frames), exit (an iris, a flash or a hard cut).** On average there is one
gag every 1.5-2 s.

---

## 2. The beat ladder

These are the gaps measured between consecutive comic events (punch to
reaction, reaction to caption, and so on). Values are in frames.

```
2 2 3 3 3 3 3 3 4 4 4 | 5 5 6 6 6 | 7 8 8 8 | 10 10 10 | 12 12 13 14 14 | 15 16 | 18 18 | 21 | 26
```

26 of the 34 gaps (76%) fall within one frame of a Fibonacci rung
(2, 3, 5, 8, 13, 21). Treat the ladder as the default vocabulary.

| rung | frames | ms | name | trailer examples | use it for |
|---|---|---|---|---|---|
| R1 | 1 | 33 | TICK | flash on a freeze (s08:14), the "tik" eye sound | shutter, click |
| R2 | 2 | 67 | BLINK | letter stagger; asterisk after the side-eye; gap after the hell insert; s05 zoom flash | the minimum readable gap |
| R3 | 3 | 100 | REACT | freeze to side-eye (s01), pop to "(cozy.)", "(We judge a little.)" to Fuzz, tail insert length, impact shake | an instant reaction, a footnote |
| R5 | 5 | 167 | BEAT | click to zoom (s05), eye to caption, spark to sign, iris close | cause to effect |
| R8 | 8 | 267 | TAKE | beep to "...it's fine." (s04), drip to 2nd sticker, look at camera to happy (s15), polaroid stagger | a dawning take, a look to camera |
| R13 | 13 | 433 | HOLD | fail x3 to SOUL LOST, a cloud reveal to its caption (14), crack after the scratch (14) | the pause before a deadpan line |
| R21 | 21 | 700 | PAUSE | caption to iris (s05, 21), the starer still after his caption (22) | uncomfortable stillness |
| R26 | 26+ | 870 | DEADPAN | "We don't judge" to "We judge a little" (26), "Yes, the actual..." to "The grey one" (18) | a double-beat deadpan |

Proposed constants (§7.2): `BEAT_TICK 1, BEAT_BLINK 2, BEAT_REACT 3, BEAT_BEAT 5, BEAT_TAKE 8, BEAT_HOLD 13, BEAT_PAUSE 21, BEAT_DEADPAN 26`.

---

## 3. Technique catalog

Each entry gives where the technique appears in the trailer, its exact numbers, the rule, a
fixed-point recipe, and how the game uses it. Costs are summarised in §8.

### 3.1 The three entrances: DROP, POP and SLAM (anticipation and overshoot)

The trailer has no generic "ease-out-back". Every entrance is one of three
per-frame tables:

**DROP** (an object arrives from above). The fall is ease-in `t^2` with
`t = (a+1)/N`. Because of the +1, the contact frame is the last frame of the
fall, still stretched. The next frame squashes.

| use | fall N | height | stretch while falling | impact table (sx / sy / dy px) |
|---|---|---|---|---|
| s01 heart | 14 | 120 px | sx 0.72 (sq -0.28) | sq {0.34, 0.24, 0.08}, then bounces of 18 px/10 f and 7 px/6 f, squash 0.20 / 0.08 between them, settle 0.06 for 2 frames |
| s02/s15 title letters | 7 | 80 px | 0.82 / 1.25 | SX {1.38, 1.20, 0.90, 0.96, 1.03, 1.0}, SY {0.62, 0.80, 1.12, 1.05, 0.98, 1.0}, DY {0, 0, -4, -4, -1, 0} |
| s01 "HaHaland" letters | 3 (dy table) | 18 px | 0.80 / 1.25 | SX {1.30, 1.12, 0.94, 1.03, 1.0}, SY {0.72, 0.90, 1.08, 0.98, 1.0}, DY {0, 0, -3, -1, 0} |
| s14 handheld | 8 | 150 px | 0.85 / 1.18 | SX {1.22, 1.10, 0.95, 0.98, 1.02, 1.0}, SY {0.78, 0.90, 1.07, 1.03, 0.99, 1.0}, DY {6, 3, -3, -2, 0, 0} |
| s04 device | 7 | 150 px | - | B (dy) {5, 3, -3, -3, -1, 1, 0} |
| s12 park sign | - | - | - | SG (dy) {-14, -6, 2, 0, -1, 0} |
| s08 crown | 8, ease-**out** `(1-k)^2` | 90 px | - | 5 px sine bounce over about 5 frames, then **snaps to a 0.25 rad tilt** (crooked) |

**POP** (appears in place, starting small):

| use | Q8.8 table |
|---|---|
| speech bubble (s04) | {102, 307, 236, 266, 256}, text shows from age 2 |
| subtitle letter (s02/s15) | {102, 333, 230, 269, 256}, dy -3 on frame 0 |
| "SOUL LOST." (s11) / s14 labels | {77, 333, 236, 269, 256} |
| lockup numbers (s12) | {51, 346, 230, 271, 251, 256}, the word follows 3 frames later |
| third eye open (s05) | {77, 346, 218, 282, 256} |
| menu window (s11) | {38, 141, 287, 243, 264, 256} |
| post-credits blob (s15) | {51, 320, 230, 269, 256} |
| heart burst (s06) | {77, 205, 410, 486, 410, 410}, ends big (1.6x) and keeps breathing |
| "!" bang (s08) | height 12 px x `tk_bounce(a/4)` |

**SLAM** (lands on the screen, starting large):

| use | Q8.8 table |
|---|---|
| "EXORCISM FAILED" stamp | {614, 410, 236, 266, 256} |
| xN counter | {563, 384, 218, 282, 248, 256}, x1.2 on the 3rd failure, plus 1 px jitter for 20 frames |
| sticker #1 / #2 (s02) | {410, 320, 236, 266, 256} / {461, 333, 243, 256} |
| slam caption (s08) | `k = 1 + (1 - bounce(a/7)) * 1.4`, giving 2.40, 1.50, 0.94, 0.81, 0.88, 0.97, 1.02, 1.0 |
| slam caption, big text (s10) | `* 0.2` instead of `* 1.4`, giving 1.20, 1.07, 0.99, 0.97, 0.98, 1.0 |
| polaroids (s08) | `1 + (1 - bounce(a/8)) * 0.6`, 8 frames apart, 3-frame white bloom |

**Anticipation** (wind-ups) uses these ratios of anticipation to action to
follow-through:

* **s14 high-five: 4 : 6 : 12.** The arms pop out with `bounce(a/7)`. The
  hands pull back 6 px and lift 12-14 px for 4 frames, swing in with
  smoothstep over 6 frames, hit on the clap frame, and impact lines spread
  for 12 frames.
* **s07 inhale: 10 : 5 : 1+8.** A stretch of `sy 1 + 0.12 k`,
  `sx 1 - 0.06 k` (smoothstep, 10 frames), then puffed cheeks held for 5
  frames (1.08 / 1.03) with +/-1 px jitter, then the release on one frame
  and a squash sy 0.88 to 1 (`bounce/8`).
* **s04 paw press: 8 : 4 : 10.** Smoothstep in, held with a 2 px press,
  smoothstep out.
* **s11 cursor click:** +2 px for the 2 frames *before* the toggle flips.
  That is a micro-anticipation.

**Rule.** Comic entrances use tables or springs that start from rest
(§3.2), not back-ease. Back-ease front-loads velocity and loses the
trailer's "gathering" feel. Fitting `easeOutBack` to the s05 window boing
gives 6x the error of a spring fit. Use back-ease (`s = 1.2`, about 5%
overshoot) only for plain UI slides.

**Recipe.**

```c
/* Q8.8 per-frame table; before = table[0], after = table[n-1] */
static inline int tw_tab(const int16_t *t, int n, int age)
{ return age < 0 ? t[0] : age >= n ? t[n - 1] : t[age]; }
/* drop:  y = rest - H*(1 - t^2), t = (a+1)/N in fix16 */
fix16 t = FIXDIV(INT2FIX(a + 1), INT2FIX(N)); fix16 y = FIXMUL(INT2FIX(H), FIX_ONE - FIXMUL(t, t));
```

### 3.2 Elastic springs (`tk_bounce`, analysed)

`tk_bounce(t) = 1 - cos(6t) * e^(-4t) * (1 - 0.2t)` (`trailer/tk.c:470`)
is the trailer's only elastic curve:

* **Overshoot.** The peak is **1.139 at t = 0.42**, the trough is **0.985
  at t = 0.95**, and `bounce(1) = 0.986`. The 1.4% snap at the end goes
  unnoticed.
* **Damping.** It is a damped oscillator with **zeta = 4/sqrt(16+36) =
  0.555**. Over N frames its period is **1.047 x N frames**.
* **Samples.** N=4: {0, .975, 1.121, 1.009, .986}. N=7: {0, .641, 1.043,
  1.139, 1.086, 1.020, .989, .986}.

Fixed-point LUT (33 entries, `t = i/32`, interpolate linearly, Q8.8):

```c
static const uint16_t TW_BOUNCE_Q8[33] = { 0,35,73,110,145,177,206,230,250,265,277,284,289,291,291,290,
                                           287,283,279,275,271,267,264,261,258,256,255,254,253,252,252,252,252 };
```

The fix16 version is in Appendix B. Smoothstep, the trailer's `tk_ease`,
at 17 points in Q8.8:
`{0,3,11,24,40,59,81,104,128,152,175,197,216,232,245,253,256}`.

**Runtime springs** handle interactive things: a dragged piece of
furniture, the camera, a cursor, a punch. Use the *exact* sampled damped
oscillator. It is a two-tap recurrence, stable for any period, with two
multiplies per axis per tick:

```
e[n+1] = a1*e[n] + a2*e[n-1],   a1 = 2r*cos(wd),  a2 = -r^2,
wd = 2*pi/period,  r = e^(-zeta*wd/sqrt(1-zeta^2)),  pos = target + e
```

Positions are Q4 (1/16 px) and coefficients are Q14. The product fits in
int32 for |e| < 4096 px, so no 64-bit multiply is needed on ARM7 or R3000.
The presets below were fitted to the trailer and checked in integer math by
`story/tools/springs.py`:

| preset | period (f) | zeta | A1 (Q14) | A2 (Q14) | 64 px step response (px per frame) | fitted to |
|---|---|---|---|---|---|---|
| `TW_JELLY` | 3.8 | 0.41 | -1287 | -3705 | 0, 52.9, 79.3, 65.3, 60.4, 63.9, 64.8, 64 | the s02/s15 letter SX table (error 7e-5) |
| `TW_SLAM4` | 4.2 | 0.555 | 903 | -2226 | 0, 46.6, 71.7, 66.8, 63.1, 63.6, 64 | `bounce(a/4)` ("!") |
| `TW_SLAM7` | 7.3 | 0.555 | 12029 | -5196 | 0, 22.2, 53.6, 69.6, 71.3, 67.6, 64.2, 63.0 | `bounce(a/7)` (slam captions) |
| `TW_SLAM8` | 8.4 | 0.555 | 14583 | -6039 | 0, 17.9, 46.5, 65.4, 71.6, 70.2, 66.8 | `bounce(a/8)` (polaroids, weapon raise, lid) |
| `TW_BRAKE` | 8.4 | 0.45 | 16477 | -7710 | from e0 = +6 and v0 = 6.4: 6.0, 8.4, 5.6, 1.6, -1.1, -1.9, -1.4 | the s03 brake table {6, 8, 6, 2, -1, -2, -1, 0} |
| `TW_BOING` | 11 | 0.71 | 15498 | -5178 | 0, 14.1, 37.0, 54.2, 63.2, 66.4, 66.4, 65.5 | the s05 window boing (out-back fits 6x worse) |
| `TW_WOBBLE` | 8 | 0.22 | 19409 | -11497 | 49% overshoot, long ring | furniture wiggle (apply it to squash, x0.2) |
| `TW_FOLLOW` | 24 | 0.85 | 20746 | -7039 | 0, 5.9, 17.9, 30.6, 41.4, 49.8 ... no overshoot | lazy camera follow, cursor glide |

Integer recipe:

```c
typedef struct { int32_t e, ep, target; int16_t a1, a2; } TwSpring; /* Q4 px, Q14 coeffs */
static inline int32_t tw_spring_step(TwSpring *s)
{ int32_t n = (s->a1 * s->e + s->a2 * s->ep) >> 14; s->ep = s->e; s->e = n; return s->target + n; }
/* retarget, keeping velocity: e -= (new - old), ep -= (new - old)
 * punch (offset, then elastic return): e += d; ep += d
 * shove (velocity impulse): ep -= dv                                  */
```

The first sample after a step from rest is `e1 = r*(e0*cos wd + (sigma*e0/wd)*sin wd)`.
Precompute it per preset for an exact match to the trailer curve. Starting
with `ep = e0` is simpler but gives a snappier first frame: BRAKE toward 64
px goes 29.8, 59.6, 75.7, 77.8, 72.4 px instead of 16.8, 46.6, 68.7, 76.9,
74.7 (checked in `story/tools/ctest/t.c`). The LUT `tw_bounce` stays within
0.2% of `tk_bounce`.

### 3.3 Squash and stretch ratios

* **Volume.** The trailer does **not** keep area constant. sx*sy dips on
  the impact frame and then returns to 1.0 within 2 frames:
  * title letters: 0.856, 0.96, 1.008
  * s01 letters: 0.936
  * handheld: 0.952
  * heart: 0.90 stretched, 0.93 squashed
  The loss of mass on the impact frame is part of the snap. The heart rule
  is `w = W*(1+sq)` and `h = H*(1 - 0.9*sq)`.
* **Peak deformation by "material":**

  | material | peak deformation |
  |---|---|
  | candy letters | 38% |
  | plush heart | 34% |
  | plush critter landing (s07 hop) | 22% (1.22 / 0.80) |
  | plastic handheld | 22% |
  | console (metal) | 18% (`clunk` {0.82, 0.92, 1.06, 1.02, 1.0}) |
  | clap recoil | 10% |

  Rule: **softer and cuter means more squash.** Hellvile hardware stays
  stiff. **Hell elements do not wobble at all.** In s02 and s15, "the HELL
  part does not wobble. it drips."
* **Hop cycle** (s07, a critter hopping in). The hop is 7 frames long with
  a 12 px sine arc:
  * the landing frame squashes to 1.22 / 0.80;
  * frames 1-2 stretch to 0.86 / 1.16;
  * the final landing lerps from 1.2 / 0.82 to 1.0 over 4 frames.
* **Idle breathing** (`tk_chibi.c:412`): the body ry changes by +/-0.25
  design units, about 1 px at size 2. The architecture rule is "1 px squash
  and stretch".
* **Anchor.** Squash always scales around the **feet / contact point**:
  `layer_draw(..., x, yfeet, sx, sy)` in s07. It never scales around the
  centre.

### 3.4 Smear frames and whip inserts

The trailer uses three cheap kinds of smear instead of motion blur:

1. **Over-extended in-between.** The devil tail (`s01:494`) is 3 frames
   with reaches of 29 px, **44 px** and 27 px. The middle frame is 1.5x too
   long. Hand-drawn, one frame.
2. **Growth-on-approach.** The flung drip (`s02`) travels for 6 frames
   (`t^2`) while its radius grows from 3 to 20 px. A lob toward the lens
   reads as speed.
3. **Speed lines on the cut frame.** In the s05 close-up, radial lines
   alternate phase every frame (`(a&1)*0.8`) for the whole shot. Fleeing
   citizens throw sweat drops behind them every 3rd frame. s13 fireworks
   draw a 1-pixel trail at 85% of their path.

**Exits accelerate.** Fleeing citizens move `x = dir*(2a + 0.35a^2)`,
staggered by `seed%7`. They never ease out. The ghost wanders off with
`1.1a^2`.

### 3.5 Smash cuts

* **Flash cut.** One frame of white flash at level 10/16 (s09 split, s05
  zoom 10 then 5). A 3-frame glitch (strength 30) on a mode switch (s10
  cute to brutal).
* **Covered cut.** In s07 the FWOOMP cloud fills the screen for 22 frames.
  The transformation happens behind it, and two red eyes glow in the fog
  first. Use this for any transformation the player shouldn't see happen:
  a possession, a room makeover, a costume change.
* **Insert cut.** See §3.16.
* **Rule.** The audio cuts on the **same frame** as the picture. Every
  trailer cut has an sfx on frame 0 (WHOOSH, GLITCH or THUD).

### 3.6 Hard freeze-frame with caption

There are two variants, both made by **clamping animation clocks**, not by
pausing the game:

* **s08 freeze (`s08:488`).** At F_SCRATCH:
  * every citizen's bob clock is clamped (`bob = F_SCRATCH`) and the
    confetti stops;
  * hopping citizens **hang in the air** (cartoon physics) for 14 frames
    until the sky cracks;
  * one frame of white flash at 7/16 ("camera shutter");
  * expressions switch to SHOCK 2 frames later;
  * "!" bangs ripple in over 4 frames (`seed % 4`);
  * 14 frames of musical silence.

  After the crack, the clock runs at **2x** (`bob = f*2`): panic breathing.
* **s01 heart (`s01:422`).** It stops breathing at F_FREEZE (the squash
  term is held), the shine sweep and hover stop, and only the eyes move.

**Recipe.** Give each actor its own animation clock:
`clk = frozen ? freeze_at : panic ? 2*f : f`. The beat scheduler (§7.2)
owns `FREEZE`, `THAW` and `PANIC`.

### 3.7 Holding the beat (how long before the punchline)

| situation | hold | where |
|---|---|---|
| silence after an impact, before the gag's resolution | 10 frames | s02 splat to sticker |
| freeze before the reaction | 3 frames | s01 |
| reaction before the deadpan line | 8 frames | s04 beep to "...it's fine." |
| a reveal before its caption | 14 frames | s07 cloud parts to "Everything is fine." |
| caption to a tiny wrongness (the button) | 22 frames | s06 starer caption to slow wave; 24 frames s07 caption to balloon pop |
| double-beat deadpan | 18-26 frames | s14, s10 |
| landing before a decision | 4 frames | s05 lands on Kevin, then the click |
| post-credits beats | 7, 8, 9, 6 frames | s15 pop, strain, plop, horns, beep |

**Rule.** Fast setups, then the pause. Before a punchline the screen must
be **readable and still**: the music stops, the camera stops creeping (s10
stops walking at F_JUDGE1, the weapon bob drops to 0.3x), and idle motion
stays on (or goes off, for horror).

### 3.8 Record scratch, silence, then the sting

* **Visual scratch (s03:933).** The pan accelerates (quadratic for the first
  30%), then runs at **constant velocity** (about 5.2 px per frame) right
  into the brake. There is no ease-out: the stop is sudden. The overshoot
  table is O = {6, 8, 6, 2, -1, -2, -1, 0} px (the `TW_BRAKE` spring), and
  a 3-frame x-shake of {+1, -2, +1}. After that the camera creeps forward
  0.04 px per frame (dread).
* **Audio scratch (music.c:532, 676).** Noise sweeps from 3000 down to 400
  Hz over 0.12 s, then from 400 back up to 3000 Hz over 0.18 s. The
  **tape-stop** slides the lead down 15 semitones (79 to 64) over 0.16 s
  (5 frames) and the bass down 12 (48 to 36) over 0.2 s (6 frames). Then
  silence: 14 frames in s08, 64 frames in s03.
* **The sting comes after the silence and is small.** A 2-note "beep." (s08
  +7), a "dun" (s15 horns), a single *tink*. Big stings (ROAR, the slam
  captions) come only after a small one has broken the silence.
* **Re-entry.** The score behaves as if it **kept playing silently** and
  comes back in on the grid: "the vamp sneaks back in like nothing
  happened", "two-beat pickup back in". In the game, a mute should keep
  the sequencer running. Use pause only when you want "the record
  restarted".

### 3.9 Double-takes, looks to camera, reaction ripples

* **Classic double-take** (the s02 logo eyes, `s02:791`):

  | frames | eyes |
  |---|---|
  | 50-57 | look down at the drip |
  | 58-73 | shock, with nervous jitter |
  | 74-81 | stare **at the lens** |
  | 82-97 | happy (denial) |
  | 98-103 | shock again (the drip escaped) |
  | 104-107 | happy |
  | 108+ | look away and whistle |

  The beats are look (8), shock (16), stare (8), denial, re-shock (6).
* **Look to camera.** s01 side-eye: shock for 6 frames, then the side-eye,
  with a "tik" at 50 and 54. s15: shock for 14 frames, then **8 frames
  looking straight at us**, then happy plus "(it's fine.)".
* **Reaction ripple.** Crowd "!" bangs start at `+2 + (seed % 4)`, and
  stadium waves use a phase of `0.45 f - 0.5 col - 0.08 row` rad.
* **Sweat drop** for embarrassment: it starts 3 frames after the side-eye
  and slides 1 px every 2 frames.

### 3.10 Camera: snap-zoom, whip-pan, tilt holds

* **Snap-zoom (s05:695).**
  * Click; **5 frames** later, cut to the close-up with a 2-frame white
    flash (10/16, then 5/16) and radial speed lines.
  * The subject wobbles in y: {+6, -3, +1, 0}.
  * Sound: WHOOSH plus a "dun-DUN" pulse slide of 50 to 74 over 0.25 s.
  * The payoff (the eye opens) comes 4 frames after the cut. The caption
    comes 5 frames after the payoff.
* **Tilt holds (s06:893).** The camera alternates holding (with a 0.06-0.12
  px per frame drift) and moving (12-frame smoothstep). A comic camera
  never eases slowly between subjects. It sits, then moves quickly.
* **Tilt to the reveal** (s13: 16 frames of smoothstep, then the caption 12
  frames later). Use this for "the camera finds the consequence".
* **Iris transitions.** The iris is the trailer's signature transition:
  * **Out:** radius 260 to target over 5 frames (smoothstep), held at
    radius 17-26 for **3-5 frames** on a detail that moves, then black.
    The holds are s02 3, s06 4, s05 5. Stretch to 10 only if the button
    needs it.
  * **In** (the next scene): `r = 6 + 250*smooth(a/6)^2` over 6-7 frames.
    Squaring the curve makes it open slowly, then whoosh.
  * Rim: 2 px of `PAL_UI_PINK_D`. The heart shape is for cute exits, the
    circle for creepy ones.
  * Sound: a sine "shwip" sliding 96 to 72 over 0.16 s.

### 3.11 Rule-of-three escalation

The canonical example is s11 (`s11:449`, `1237-1248`, `1252`):

| attempt | frame | gap | method | stamp shake | flash | counter | kazoo |
|---|---|---|---|---|---|---|---|
| x1 | 92 | - | spray bottle | 3 frames (3, 2, 1) | white 6, 4, 2 | 2.2x slam | 1 note, 62 to 57 |
| x2 | 107 | 15 | **a real hug** | 3 frames | none | 2.2x | 1 note, 58 to 53 |
| x3 | 123 | 16 | **the whole bucket** | **5 frames** (3, 3, 2, 2, 1) | **red** 8, 5, 2 | **2.64x** plus 20 frames of jitter | **3 notes**: 6 f, 6 f, 18 f, falling 55 to 40 |
| topper | 136 | 13 | candles out, "SOUL LOST." | 3 frames | darken 1 level per frame | - | minor chord |

**Rules.**

* **Keep the interval constant** (15-16 frames). The rhythm builds the
  expectation.
* **The method escalates toward absurd sincerity** (spray, hug, bucket).
* **The pitch steps down each time.**
* **The third gets bigger in every channel at once:** duration, colour,
  scale and number of notes.
* Then comes a **topper** (a fourth, different beat) rung R13 later.

**Variants.**

* **List of three and a break.** s06: "Make friends! / Fall in love! / Eat
  cupcakes!" are shown for 34, 26 and 25 frames, then "Ignore that one."
  is held for 36 frames in silence.
* **Accelerating three.** s05 changes look every 4 frames, then every 3,
  then stops dead. s09's six parts land with gaps of 10-9-8-8-6, and the
  7th "part" is the fire.
* **Equal steps, then a deflation.** s12 shows 1000 / 500 / 500 (one every
  22 frames), then "1 PARK" and the cork.
* **Escalating to the personal.** s08's polaroids, 8 frames apart: the
  bath, the bakery, **your bed**.

### 3.12 Deadpan cutaways

| shot | stillness | sound | text style | hold |
|---|---|---|---|---|
| s06 the starer | `bob = 0`: no breath, no blink, ever; lit by a cold light from nowhere | music muted for 41 frames | "Ignore that one." (normal caption, cool outline) | 22 frames, then a slow wave (8-frame phases) |
| s07 possessed bunny | `bob = 0`; the head tilts -0.14 rad (smoothstep, 6 frames) with a creak | silence after one wrong low chord | `tk_caption` **slides up 8 px (8, 6, 4, 2, 0), no bounce** | 24 frames, then the balloon pops |
| s11 demon close-up | slow bokeh drift | music muted; drip, drip at +3 and +9 | **tiny 1x text that simply appears**: "ow. my everything." | 22 frames |
| s14 "the actual PlayStation" | the scene keeps idling | the music stops dead for 38 frames | a small 1x caption | 18 frames, then "The grey one." in a **greyer** colour |

**Rules.**

* Deadpan text never hops or pops. It types instantly or slides with
  ease-out only.
* The quieter the line, the smaller the text.
* The actor's idle clock is set to 0.
* The button is something tiny moving in a still frame (a wave, a blink, a
  balloon).
* Contrast the deadpan reply with a shouted setup. s11's "THE POWER OF
  CUDDLES COMPELS YOU!" is a dialogue box at 1.5 chars per frame with "!"
  marks flashing every 3 frames, and the answer is a tiny, instant "ow."

### 3.13 Text pop-ins with bounce

* **`bcap` per-letter hop** (`s03:234`). The hop table is
  HOP = {-7, -4, +1, +2, +1, 0} px x scale/2. At 1x that is {-3, -2, 0, 1,
  0, 0}. Letters start at `age - k/2`, so **2 letters per frame**; callers
  pass `age*2` to get 4 per frame.
* **Constant reveal sweep.** Scale-2 captions run at 2 chars per frame
  (2 x 12 px) and scale-1 captions at 4 chars per frame (4 x 6 px). Both
  are **about 24 px per frame** of horizontal reveal. On PS1 use about 32
  px per frame.
* **Small tags.** "\*mostly", "(cozy.)" and "(it's fine.)" use dy {-3, +1,
  0} or {-2, +1, 0}.
* **Dialogue box** (`tk_dialog`). Typing runs at 1.5 chars per frame. A
  "next" heart blinks every 8 frames once the line is fully typed.
* **Rule.** Setup lines type, punch lines pop, deadpan lines appear.
  Big shouted words slam (§3.1).

### 3.14 Shake profiles

All shakes use the same sign pattern, `x = (f&1) ? +A : -A` and
`y = (f&2) ? +A/2 : -A/2`. That is a 4-frame Lissajous: it reads as a
rattle, not a 2-frame flicker. Edges clamp, so no black bars show.

| profile | frames | x amplitude per frame | y amplitude | source |
|---|---|---|---|---|
| `SHK_TAP` | 2 | 0 | +3, -1 | s12 caption landing, s02 LIFE slam (+2, -1), s14 landing (0, 2) |
| `SHK_BRAKE` | 3 | +1, -2, +1 | 0 | s03 record scratch |
| `SHK_POW` | 3 | 2 | 1 | s06 POW, s14 clunk (y 2), s10 Fuzz shout (y 0) |
| `SHK_IMPACT` | 3 | 3 | 2 | s02 splat, s04 hell insert |
| `SHK_STAMP` | 3 | 3, 2, 1 | 2 | s11 fail x1/x2, SOUL LOST |
| `SHK_STAMP3` | 5 | 3, 3, 2, 2, 1 | 2 | s11 fail x3 |
| `SHK_SLAM` | 5 | 3 | 1 | s08 caption slams |
| `SHK_IGNITE` | 5 | 3 | 2 | s15 HELL ignites, s11 MONSTER MODE (4 frames) |
| `SHK_SHOTGUN` | 7 | 7, 6, 5, 4, 3, 2, 1 | A/2 | s10 super shotgun |
| `SHK_FWOOMP` | 8 | 4, 4, 3, 3, 2, 2, 1, 1 | 2 | s07 |
| `SHK_CRACK` | 8 | 3, plus glitch strength 22, 20 ... 8 | 1 | s08 sky crack |
| `SHK_RUMBLE` | 12 | 3 (constant) | +2 / -1 | s13 wall crumbles |
| `SHK_ROAR` | 14 | 4x4, 3x4, 2x4, 1x2 | A/2 | s08 Vapula |
| `SHK_GUNFIRE` | while firing | 1 | 0 / 1 | s10 minigun |

**Flash levels** run 0..16 (ordered dither in the trailer, a smooth blend
on hardware):

* impact white: 10, 5 (2 frames);
* failure: 6, 4, 2;
* ignite or monster: lava or red at 12, 8, 4 or 10, 7, 4;
* scratch shutter: one frame at 7;
* shotgun: the scene's light level +4, 3, 2, 1;
* scene in from white: 16, 12, 8, 4.

### 3.15 Hit-stop

The trailer has **no per-hit freeze**. It uses the clock-clamped freeze
(§3.6) and the "hangs in the air for 3 frames" beat. For gameplay at 30
Hz, I derived these values from the ladder. They are recommendations, not
measurements:

| event | hit-stop (frames) | then |
|---|---|---|
| cute pop, plush hit | 2 (R2) | confetti, "\*squeak\*" slam at +2 (`bounce/4`) |
| shotgun or heavy hit | 3 (R3) | `SHK_SHOTGUN`, flash +4..1 |
| kill / gib | 4 | `SHK_IMPACT` |
| mini-boss stagger | 5 (R5) | `SHK_STAMP` |
| boss death | 8 (R8), plus one frame of white | the record-scratch silence (§3.8) |

Never go above 8 frames: at 30 Hz that already feels like a hitch. Hit-stop
freezes actor clocks (§3.6), **not** the renderer, the HUD face, particles
that have already spawned, or audio.

### 3.16 Tonal whiplash (cute, then gore, then cute)

* **s04 insert (`s04:636`).** Frames 128-131 are 4 frames: 2 of pentagram
  and 2 of demon face. The LCD fills the screen at 10 px per LCD pixel,
  with a vignette, +/-3 / +/-2 shake, and glitch on odd frames. The music
  is muted for **exactly those 4 frames** and the insert carries its own
  sound: a 4-frame saw slide 33 to 28 plus square tones.
  * **Back to cute with one detail changed:** tiny horns, and the caption
    "It loves you!" is gone.
  * 2 frames of nothing, then "beep." (134), then "...it's fine." (142).
* **s01 tail.** A 3-frame insert *behind* the mascot, then the freeze.
* **s15.** The HELL half rips in (the seam follows `bounce/10`). Tonal
  whiplash over the whole frame: the pastel half keeps sparkling while the
  hell half burns. That is split-screen tonal contrast.
* **Rules.**
  1. A subliminal insert is 3-4 frames long; 2 frames is too short to read
     and 6 too long to stay a "did I just see that?".
  2. The insert owns the soundtrack for exactly its length.
  3. Come back to the *same* shot with **one** persistent change.
  4. The deadpan acknowledgement comes on rung R8.
  5. **Never** show a cute character harmed in the cute half of the
     whiplash. The gore belongs to the demons and to the hell world.

### 3.17 Running gags (and how they mutate)

| gag | appearances (trailer seconds) | mutation per recurrence |
|---|---|---|
| "beep." | s04 12.9 and 16.0, s06 22.7 (distant), s08 33.2, s15 77.5 | normal; with horns; heard from far away; in the face of the apocalypse; post-credits, from a pooping demon pet |
| "it's fine." | s04 16.2, s07 31.5, s08 35.2, s15 74.2 | your pet; a possessed customer; a bubble mid-invasion; the game's **own logo** on fire (meta) |
| dripping red HELL | s02, s14 (on the handheld's title), s15 | a hidden drip; drips on a product shot; burns and drips forever |
| the pink party bunny | s07 to s08 to s11 to s13 | customer, possessed, failed exorcism, cured and bouncing in with a heart balloon |
| the crowned cloud | s07 (employee of the month), s09 (in the window), s08 (Lord Vapula) | a background prop becomes the villain |
| Kevin and the starer | s05, s06, then both at the s13 party | one-shot gags come back to the ending |

**Rules.**

1. **Establish small.** The first "beep." is a tiny bubble.
2. **Repeat within about 15-20 s, nearly identical,** so the audience
   recognises it.
3. **Then change the context, not the gag.** The words stay exactly the
   same while the stakes escalate.
4. **The last occurrence breaks the frame** (logo, post-credits).
5. **Callbacks of one-shot gags belong at the ending.** That is how the
   story pays off its cast.

### 3.18 Buttons and the iris hold

Every scene ends on a tiny after-beat:

* the escaped drip (s02 +22 after the splat);
* the cyclops waving back (s03);
* the horns (s04);
* the lone third-eye blink **inside the iris hold** (s05:686);
* the starer's slow wave **inside the iris** (s06);
* the balloon pop (s07);
* the crooked crown (s08);
* the tail still burning (s09);
* "tsk." (s10);
* the ghost reading its map upside down (s11);
* "(cozy.)" (s12);
* the tink bouncing at +6, +10 and +13 with falling volume, gaps 4 then 3
  (s13);
* the hands that stay up and wiggle (s14);
* the poop that also grows horns (s15).

**Rule.** The button is a callback or a "one more thing" smaller than the
punch. If there is an iris, the button goes in its 3-5 frame hold (up to
10).

### 3.19 The baseline: "nothing is ever static"

`tk_chibi` idles on **incommensurate periods**, so a crowd never syncs
(`tk_chibi.c:411-419`):

* **breathing:** `sin(0.16 t)`, a period of **39.3 frames**;
* **hair lag:** -0.9 rad behind the head bob (secondary motion);
* **tail wag:** a period of **22.4 frames**;
* **ear twitch:** a 97-frame cycle, left ear at 40-45 and right ear at
  70-75, each 6 frames of `0.22*sin`;
* **blink:** 3 frames every **83**.

Each actor gets a phase offset (`bob = f + i*9`, `seed*5`). Possessed
characters twitch their ears at 1.7 rad per frame (wrong, fast). Characters
in a panic run their clock at 2x.

Keep this baseline in the game. **All the deadpan and horror tools only
work because the world is otherwise alive.**

---

## 4. Audio timing (the other half of every joke)

**The silence map.** The score is muted for these windows (`music.c`,
`mute()`):

| window | frames | s |
|---|---|---|
| s01 freeze + footnote | 46-75 = 29 | 0.97 |
| s02 fling, splat, sticker | 68-94 = 26 | 0.87 |
| s03 record scratch, eyes | 96-160 = 64 | 2.13 |
| s04 hell insert | 128-132 = 4 | 0.13 |
| s05 lands on Kevin, zoom | 100-124 = 24 | 0.80 |
| s06 the starer | 109-150 = 41 | 1.37 |
| s07 inhale to possessed | 92-165 = 73 (a raw diminished swell holds through the inhale) | 2.43 |
| s08 scratch | 14-28 = 14 | 0.47 |
| s10 "we judge" | 135-195 = 60 | 2.00 |
| s11 close-up to the cursor sneaking | 60-174 = 114 | 3.80 |
| s13 shell casings | 98-150 = 52 | 1.73 |
| s14 deadpan | 74-112 = 38 | 1.27 |
| **total** | **539 frames = 18.0 s = 23% of the trailer** | |

**Rules.**

* **Mute the music, not the sound effects.** During a mute, notes that
  start inside the window are dropped, and notes running into it are cut
  at its start. Sound effects stay.
* **Duck under the joke's little sound.** The ducks are 0.4-0.75 of full
  volume and last 6-25 frames: under "beep." (0.55 and 0.5), the distant
  beep (0.4), imp gibbering (0.7), Father Fuzz yelling (0.6), "!" (0.55)
  and the power-on (0.75).
* **One sound per beat, on the same frame.** Visual beats and sound
  effects are frame-locked (`AT(F_X)`). Reactions get tiny sounds:
  * "tik" for the eyes: a 0.04 s square at note 91;
  * "plink" for the asterisk;
  * "shwip" for an iris (sine 96 to 72 over 0.16 s);
  * a gulp sine at note 79 for 0.12 s;
  * a "huh?" sine sliding 79 to 74.
* **Babble rhythm for deadpan.** "ow. my everything." plays 5 low
  syllables at +0, 6, 10, 13 and 16 frames. **A period holds 6 frames**,
  and the rest are 3-4 frames apart.
* **Sad trombone.** The kazoo slides 67 to 72, then 72 to 60. On a failure
  it is one note, getting lower each time. The third failure gets three
  notes (§3.11).
* **The coupling hazard.** `music.c` duplicates scene frame constants with
  `FR(n)`. The README warns to update both when a beat is re-timed. **The
  game should not repeat this:** sound cues must hang off the same named
  beats as the picture (§7.2).

The trailer's synth sound recipes are in `trailer/tk.c:500-519`. The
game's `SFX_*` list has no comedy kit yet. Port SCRATCH, BOING, POP, THUD,
WHOOSH, KAZOO, BEEP (pet), GLITCH, TIK, SHWIP (iris), TINK, FANFARE and
CHOIR. They are short noise and pulse recipes: on GBA they cost tone-channel
time or a few hundred bytes of PCM each, and on PS1 a few KB of ADPCM.

---

## 5. Making it feel long without losing the timing

The trailer packs about 45 gags into 78 s, roughly one every 1.7 s. A 15-25
hour game cannot run at that density, or it turns into noise. Scale the
structure, not the density.

1. **One trailer scene = one quest arc.** Each side mission follows §1's
   anatomy, but stretched:

   | trailer beat | in a quest |
   |---|---|
   | SETUP | the request, 30-60 s of dialogue and play |
   | ESCALATION | three attempts or steps, rule of three, minutes of gameplay |
   | PUNCH | a 2-5 s scripted beat at trailer density |
   | BUTTON | a **callback 1-10 minutes later**, when you pass the NPC again, or a prop changes in their house |

   That callback distance is what makes the world feel long and connected.
2. **A density curve per context:**

   | context | gag cadence | notes |
   |---|---|---|
   | cutscene (story beat) | 1 per 1.5-3 s | trailer density, with the ladder and silence map |
   | dialogue | 1 punchline per 3-6 lines | punch lines pop; at most 1 deadpan per conversation |
   | overworld walking | 1 ambient micro-gag per 20-40 s | an NPC double-take, a running-gag sighting, a prop gag |
   | housing / decorating | 1 reaction per placement, then a cooldown | variant pools of 3 or more; never repeat within 5 min |
   | combat | per-hit juice every hit; jokes only between waves | hit-stop and shake tables |
   | heartfelt scene | **0 for 3-5 s minimum**, then optionally one soft button | see 4 |
   | horror scene | none: the same holds with no button | see 3 |
3. **Scary is comedy timing with the release removed.** It uses the same
   tools:
   * a clamped idle clock (the starer), holds of R21 or longer, and silence;
   * anticipation with no release, or a release that is a *small wrong
     thing* (a slow wave, a head tilt with a creak);
   * the hell insert stays at 3-4 frames, but the "it's fine" never comes.
4. **Heartfelt is the same with the button withheld.** s13 plays the anthem
   and the hug straight for 34 frames before the tilt and the "(after a LOT
   of shooting.)" undercut.
   * In the game, undercut at most half of the sincere beats.
   * **Never undercut the final story beat** (the wall coming down and
     everyone mingling in Hellvile Park). Put the button **after the
     credits**, as s15 does.
5. **Running gags across chapters.** Map the trailer's 15-20 s recurrence
   to about 1-3 hours of play, using §3.17's mutation rules.
   * Example, the beep: chapter 1 has a normal "beep."; chapter 2 a beep
     with horns; chapter 3 beeps mid-invasion; chapter 4 beeps at the
     festival; post-credits, the poop grows horns.
   * "it's fine." escalates from pet, to possessed neighbours, to town
     signage, to the title screen itself once you beat chapter 3.
6. **Silence budget.** Aim for about 20-25% music-free time *inside
   cutscenes* (the trailer is at 23%). The overworld always has music, but
   it ducks and tape-stops for gags.
7. **Recover after big beats.** After a scene, the trailer holds an iris or
   a long settle (the s05 iris lasts 10 frames, s06 9 frames) before the
   next joke starts. In the game, give the player 5-15 s of control with no
   scripted gag after a big beat.

---

## 6. Mapping to game systems

### 6.1 Cutscenes (`core/src/cutscene.c`)

Today a cutscene is a list of `Panel { who, min_t, text }`: captions with a
minimum time. Move to beat scripts (§7.2) with this template:

```
L_SETUP     panels type at the normal speed, idle on, music on
L_ESCALATE  intervals shrink by 1-2 frames per step, or magnitude grows (shake, flash, scale)
L_PUNCH     <= 4-frame event + SFX on frame 0 + MUSIC_MUTE (or TAPESTOP)
L_REACT     +R3 (instant) / +R8 (dawning) expression change, "!" ripple
L_TAG       +R3..R13 caption: pop (cute) or appear (deadpan)
L_BUTTON    +R13..R21 tiny after-beat
L_EXIT      IRIS_OUT 5 frames + hold 3-5 frames (up to 10; the button goes here) | flash cut | covered cut
```

**Skipping.** START skips the whole cutscene. A completes typing, but
**never shortens a HOLD in a punch beat** (the hold is the joke). In the
remaining holds, A advances only after the hold's minimum time.

### 6.2 Dialogue (`core/src/ui_dialog.c`)

* **Per-glyph hop on reveal.** Use HOP1 = {-3, -2, 0, 1, 0, 0} px on each
  glyph's first 6 frames. Text speed stays as `OPT_TEXT_SPEED` sets it
  (0.5, 1, 2 chars per tick, or instant). `OPT_UI_BOUNCE` scales the hop:
  0, 1/2 or full.
* **Punctuation timing** (comma 3 and period 6 come from the "ow." babble):

  | punctuation | hold (frames) | extra |
  |---|---|---|
  | `,` | 3 | |
  | `.` | 6 | |
  | `!` | 4 | the box hops -2, +1, 0 |
  | `?` | 5 | the portrait tilts |
  | `...` | 5, 5, 5 | |
  | a new line after `...` | R8 | |
* **Inline beat codes** (the parser skips them; they compile to beat ops):

  | code | effect |
  |---|---|
  | `{p N}` | pause N frames |
  | `{.}` `{..}` `{...}` | pause R5 / R8 / R13 |
  | `{d}` | deadpan for the rest of the line: instant reveal, no hop, no babble pitch movement, duck music 0.5 |
  | `{!}` | a sting: `SHK_POW`, a "!" bang (`bounce/4`) over the speaker, a 1-frame flash of 5/16 |
  | `{big}word{/}` | the word pops whole with the bubble table |
  | `{w}` | a wavy text line: `ui_wave(t + i*3, 16, 1)` |
  | `{r}` | record scratch: tape-stop, 14 frames of mute |
  | `{cut N}` | smash cut to insert N for 4 frames |
  | `{punch}` | marks the line as a punchline: A cannot skip its hold |
* **The player's A press can be the cut.** If a punch follows a choice or a
  confirm (s05: click, then R5, then zoom), the press **ends the setup**,
  and the punch fires on rung R5 after it. Never put the press in the
  middle of a punch.
* **Portraits.**
  * A new line starts with a talk bob, squash {1.06/0.94, 1.0} over 2
    frames.
  * A shout stretches 0.92 / 1.10 for 2 frames, then `TW_JELLY`.
  * A deadpan line holds the portrait's animation clock (`bob = 0`).

### 6.3 Overworld (`overworld*.c`, `story_talk.c`)

* **Keep the idle baseline** (§3.19) on every NPC, with id-hashed phases.
* **NPC double-take** when the player passes within 2 cells:
  1. a stop of R3;
  2. a head turn over 2 frames;
  3. a "!" bang (`bounce/4`, 12 px);
  4. an emote bubble pop {0.4, 1.2, 0.92, 1.04, 1.0};
  5. "tik".

  Rate-limit it per NPC (once per visit).
* **The border record scratch.** Crossing from Hillvile into Hellvile
  triggers a 5-frame tape-stop, 14 frames of silence, then the Hellvile
  theme on a downbeat. The camera brakes with `TW_BRAKE` if it is
  following.
* **Running-gag spawner** (a weighted table with cooldowns):
  * the starer in a random apartment window, perfectly still, waving only
    if you stand still for R21;
  * the crowned cloud as a background prop;
  * a pet "beep." at the worst moment (during a sincere line);
  * the town "DAYS SINCE LAST FIRE" sign (§6.4).
* **Possessed citizens:** `bob = 0`, ears twitching at 1.7 rad per frame,
  a grin; dialogue gets `{d}`.

### 6.4 Housing: visiting, decorating and building the dream room

Housing is isometric on GBA and 2.5D on PS1. Every interaction gets the
trailer's DROP / POP / SLAM vocabulary:

| action | timing | numbers |
|---|---|---|
| enter your home | heart iris in | 6 frames, `r = 6 + 250*smooth^2`, door boing `TW_JELLY` on the door sprite |
| place an item | DROP | fall 6 frames `t^2` from 24 px (GBA) / 32 px (PS1), stretch 0.85/1.18 while falling; impact SX {312, 282, 243, 251, 261, 256}, SY {200, 230, 274, 264, 253, 256}, DY {6, 3, -3, -2, 0, 0} (the s14 table); 2-frame dust puffs; THUD+BOING on the impact frame |
| neighbours react | secondary motion | items touching it play `TW_JELLY` at 0.4 amplitude, **2 frames later** (a ripple) |
| invalid spot | "nope" | x shake +2, -2, +2, -2 (4 frames), the item squashes sadly 0.94/1.06, SFX_ERROR. No red flash (stay cute) |
| pick up | anticipation | 2 frames of squash 1.10/0.90, then a 3-frame lift stretched 0.92/1.10, the shadow shrinks |
| rotate | squash-flip | sx {1.0, 0.5, 0.0 (edge-on), -0.5, -1.04, -1.0} (5 frames), with a swap of the facing sprite on the edge-on frame |
| wallpaper / floor change | covered cut | heart iris 5 frames out, 2 frames black, 5 frames in, **or** a citizen runs across with a paint roller (a 4-frame flee curve) covering the swap |
| room rating | counter + SLAM | the score rolls `v = N*(a/22)^2.2` (accelerating; `t^2.2 ~= t^2*(0.8+0.2t)` in fix16, under 3% error); the grade stamp {614, 410, 236, 266, 256}; three visitor reactions, 15 frames apart, escalating; a topper |
| a cursed item (pentagram rug, Vapula's crown trophy) | tonal whiplash | a 4-frame hell insert of the room (hellify plus glitch), then back with **one** persistent detail (the lamp grew horns); "...it's fine." from a visitor at R8 |
| sleep in bed (heartfelt) | no gag | a 30-60 frame fade; the button is the *next morning* (1 in 20 chance the starer is at the window) |

**Gag props for decorating** (running-gag merchandise):

* the "DAYS SINCE LAST FIRE" sign. It counts real in-game days and resets
  when Rivet visits. The flip is 7, then "-" (2 frames with jitter), then
  0 in red, pulsing every 4 frames.
* a horned-poop plushie;
* Kevin's three-eyed bust (only the third eye blinks, once every 83+
  frames);
* the "Plain Human" figurine: the only decor in the game that never
  animates.

### 6.5 Combat (`combat*.c`, the first-person and third-person views)

* Hit-stop, shake and flash come from §3.14 and §3.15.
* **The weapon swap:**
  * the old weapon drops 14 px per frame for 6 frames;
  * the new one rises with `(1 - bounce(a/8))*90` px, which overshoots
    12 px below its rest position;
  * 3 frames of glitch when switching between CUTE and BRUTAL.
* **The HUD face reacts on every beat:** firing turns it SPARKLE, a quiet
  moment HAPPY, a judgement caption SMUG, a disapproving NPC SHOCK. It is
  the player's reaction shot.
* **Rate-limited judgement captions.** After a big brutal overkill, there
  is a 1-in-N chance of "(We don't judge.)", then R26, then "(We judge a
  little.)", and Father Fuzz leans in from the side 3 frames later
  (smoothstep over 8 frames, a head shake `2*sin(0.9a)`).

### 6.6 Pet (Doomogachi)

* The hatch is a **downbeat**: the wobble period is 6 frames, then 3, and
  the hatch lands on the music's beat.
* The strain lasts 8 frames with a 1 px jitter, the plop has a 3-frame
  squash, and stink lines rise every 5 frames.
* An evolution to its bad form uses §3.16 exactly: a 4-frame insert, one
  persistent change, "beep." 2 frames later, and a deadpan caption at R8.

---

## 7. API proposal (C99, no floats, no malloc, no stdio)

The prefixes `tw_`, `beat_` and `camfx_` are unused in `core/` today. The
API **reuses** what exists: `ui_pop_scale()` (whose 12-tick table stays the
default for menus), `ui_wave()`, `ui_heart_squash()`, and the options
`OPT_UI_BOUNCE`, `OPT_SCREEN_SHAKE`, `OPT_REDUCE_FLASH`, `OPT_TEXT_SPEED`
and `OPT_SUBTITLES`.

### 7.1 `core/include/doomo/tween.h`

```c
/* tween.h - comic-timing curves, keyframe tables and springs (owner: foundation).
 * fix16 time, Q8.8 scales (256 = 1.0), Q4 spring positions, Q14 coefficients. */
#ifndef DOOMO_TWEEN_H
#define DOOMO_TWEEN_H
#include "types.h"

fix16 tw_t(int age, int dur);              /* clamp(age/dur) as fix16            */
fix16 tw_smooth(fix16 t);                  /* t*t*(3-2t)                (tk_ease) */
fix16 tw_in2(fix16 t);                     /* t^2: falls, flings, sneaks          */
fix16 tw_out2(fix16 t);                    /* 1-(1-t)^2: crown drop, cloud spread */
fix16 tw_back(fix16 t, fix16 s);           /* UI slides only; s = 0x13333 -> ~5%  */
fix16 tw_bounce(fix16 t);                  /* 33-entry LUT of tk_bounce + lerp    */
int   tw_lerp(int a, int b, fix16 t);

/* Keyframe tables: the trailer's real workhorse. One entry per frame.
 * Clamps: age < 0 -> v[0], age >= n -> v[n-1]. */
typedef struct { const int16_t *v; uint8_t n; } TwTab;
int tw_tab(const TwTab *t, int age);
/* Squash = scale pair + y offset, anchored at the feet / contact point. */
typedef struct { int16_t sx, sy; int8_t dy; } TwSquash;
TwSquash tw_squash(const TwTab *sx, const TwTab *sy, const TwTab *dy, int age, int amount256);
/* amount256 = OPT_UI_BOUNCE scaling (0, 128, 256): lerps every value toward 256 / 0. */

/* The shared library of trailer tables (Appendix A), in ROM. */
extern const TwTab TW_POP_BUBBLE, TW_POP_LETTER, TW_POP_LOCKUP, TW_POP_EYE, TW_POP_MENU,
                   TW_SLAM_STAMP, TW_SLAM_COUNTER, TW_SLAM_STICKER,
                   TW_DROP_TITLE_SX, TW_DROP_TITLE_SY, TW_DROP_TITLE_DY,
                   TW_DROP_SOFT_SX, TW_DROP_SOFT_SY, TW_DROP_SOFT_DY,   /* s14 handheld */
                   TW_HOP1, TW_TAG_DY, TW_CLUNK, TW_CLAP;

/* Springs: exact sampled damped oscillator, 2 multiplies per tick. */
enum { TW_JELLY, TW_SLAM4, TW_SLAM7, TW_SLAM8, TW_BRAKE, TW_BOING, TW_WOBBLE,
       TW_FOLLOW, TW_SPRING_COUNT };
typedef struct { int32_t e, ep, target; int16_t a1, a2; } TwSpring;
void    tw_spring_init(TwSpring *s, int preset, int32_t pos_q4);
void    tw_spring_target(TwSpring *s, int32_t target_q4); /* keeps velocity            */
void    tw_spring_punch(TwSpring *s, int32_t d_q4);       /* offset that snaps back    */
void    tw_spring_shove(TwSpring *s, int32_t dv_q4);      /* velocity impulse          */
int32_t tw_spring_step(TwSpring *s);                      /* returns position, Q4      */
#endif
```

Memory: tables total about 400 bytes of ROM, the LUT 66 bytes, the spring
presets 32 bytes. Each live spring is 16 bytes of RAM.

### 7.2 `core/include/doomo/beat.h`: the beat scheduler

```c
/* beat.h - frame-exact scripts for cutscenes, dialogue beats and overworld gags.
 * Every event hangs off a named anchor (label), so re-timing one beat drags its
 * sound, shake and captions with it (the trailer's music.c FR(n) problem, solved). */
#ifndef DOOMO_BEAT_H
#define DOOMO_BEAT_H
#include "types.h"

enum { BEAT_TICK = 1, BEAT_BLINK = 2, BEAT_REACT = 3, BEAT_BEAT = 5, BEAT_TAKE = 8,
       BEAT_HOLD = 13, BEAT_PAUSE = 21, BEAT_DEADPAN = 26 };

enum {                     /* op            a                b                    */
    BOP_END,
    BOP_LABEL,             /* label id      -                                     */
    BOP_WAIT_A,            /* label id      min frames      (fires on A press)    */
    BOP_HOLD,              /* label id      frames          (A can't skip)        */
    BOP_SAY, BOP_CAPTION,  /* speaker/style text id                               */
    BOP_POP,               /* object id     TwTab id                              */
    BOP_EXPR,              /* actor         TKE/expr | mouth<<8                   */
    BOP_ACT,               /* actor         action id (hop, wave, flee, turn)     */
    BOP_FREEZE, BOP_THAW, BOP_PANIC,  /* actor (-1 = all): clamp / resume / 2x clock */
    BOP_HITSTOP,           /* frames        -                                     */
    BOP_SHAKE,             /* SHK_* profile -                                     */
    BOP_PUNCH,             /* dx            dy  (camera spring kick)              */
    BOP_ZOOM,              /* scale Q8.8    frames                                */
    BOP_FLASH, BOP_FADE,   /* colour        level16 | frames<<8                   */
    BOP_IRIS_OUT, BOP_IRIS_IN, /* centre actor/prop id, hold | heart<<6 | (radius/2)<<7 */
    BOP_CUT,               /* shot id       -                                     */
    BOP_INSERT,            /* insert id     frames (whiplash, auto-mutes music)   */
    BOP_SFX,               /* SFX_*         volume                                */
    BOP_MUSIC, BOP_MUSIC_MUTE, BOP_MUSIC_DUCK, BOP_TAPESTOP, /* song | frames | gain */
    BOP_FLAG,              /* story flag    value                                 */
    BOP_COUNT
};
typedef struct {
    uint16_t at;           /* frames after the anchor fired                 */
    uint8_t  anchor;       /* label id (0 = script start)                   */
    uint8_t  op;
    int16_t  a, b;
} BeatEv;                  /* 8 bytes, ROM; a 60 s cutscene ~ 150 ev = 1.2 KB */
typedef struct { const BeatEv *ev; uint16_t n; uint8_t nlabels; } BeatScript;

void beat_start(const BeatScript *s);
int  beat_tick(void);              /* 0 when finished; call once per 30 Hz tick       */
int  beat_age(int label);          /* frames since the label fired, -1 if not yet:     */
                                   /*   draw code does tw_tab(&TW_SLAM_STAMP, beat_age(L_FAIL1)) */
int  beat_clock(int actor);        /* per-actor animation clock: frozen / 2x / normal  */
void beat_skip(void);              /* START: jump to BOP_END, applying flags only      */
#endif
```

**How it works.**

* `beat_tick()` advances a cursor through events that are presorted by
  anchor. An event fires when `label_frame[anchor] + at == now`.
* A label fires either on a schedule or when A is pressed (`BOP_WAIT_A`).
  Player pacing therefore only moves *anchors*, and the frame-exact gaps
  between beats inside a punch survive.
* The cost is O(events fired) per tick, about 50 cycles.

**Example.** The s04 gag as an in-game cutscene (the pet's first dark
evolution):

```c
enum { L0, L_HATCHED, L_INSERT, L_BACK, L_LINE };
static const BeatEv EV_PET_TURN[] = {
  {  0, L0,       BOP_LABEL,      L_HATCHED, 0 },
  { 40, L_HATCHED,BOP_SAY,        SPK_PET, TXT_BEEP },          /* "beep."          */
  { 40, L_HATCHED,BOP_MUSIC_DUCK, 6, 140 },                     /* 6 f at 0.55      */
  { 90, L_HATCHED,BOP_LABEL,      L_INSERT, 0 },
  {  0, L_INSERT, BOP_INSERT,     INS_PENTAGRAM, 4 },           /* mutes 4 f itself */
  {  0, L_INSERT, BOP_SHAKE,      SHK_IMPACT, 0 },
  {  0, L_INSERT, BOP_SFX,        SFX_GLITCH, 200 },
  {  4, L_INSERT, BOP_LABEL,      L_BACK, 0 },
  {  0, L_BACK,   BOP_FLAG,       SF_PET_HORNS, 1 },            /* the one persistent change */
  {  2, L_BACK,   BOP_SAY,        SPK_PET, TXT_BEEP },          /* R2               */
  { 10, L_BACK,   BOP_LABEL,      L_LINE, 0 },                  /* +R8 after beep   */
  {  0, L_LINE,   BOP_CAPTION,    CAP_DEADPAN, TXT_ITS_FINE },  /* appears, no pop  */
  { 21, L_LINE,   BOP_IRIS_OUT,   ACT_PET, 10 | 1 << 6 | 13 << 7 }, /* heart iris r=26 on the pet, hold 10 */
  { 60, L_LINE,   BOP_END,        0, 0 },
};
```

### 7.3 `core/include/doomo/camfx.h`: shake, punch, zoom, flash, iris

```c
/* camfx.h - full-screen comic post effects. Core keeps the state; the platform
 * applies it in hardware where it can (weak software fallbacks in core). */
enum { SHK_TAP, SHK_BRAKE, SHK_POW, SHK_IMPACT, SHK_STAMP, SHK_STAMP3, SHK_SLAM,
       SHK_IGNITE, SHK_SHOTGUN, SHK_FWOOMP, SHK_CRACK, SHK_RUMBLE, SHK_ROAR,
       SHK_GUNFIRE, SHK_COUNT };
void camfx_shake(int profile);                 /* x OPT_SCREEN_SHAKE (0/50/100%)  */
void camfx_punch(int dx, int dy);              /* TW_SLAM4 spring kick            */
void camfx_zoom_snap(int scale_q8, int frames);/* GBA BG2 affine; PS1: cut instead */
void camfx_flash(uint8_t color, int level16, int frames); /* REDUCE_FLASH caps it */
void camfx_fade(int level16);
void camfx_iris(int cx, int cy, int r, int heart, uint8_t rim);
void camfx_tick(void);
void camfx_offset(int *dx, int *dy);           /* for software paths              */
/* platform hooks (platform.h additions, weak no-op / software defaults) */
void plat_post_offset(int dx, int dy);         /* GBA: REG_BG2X/Y at flip; PS1: E5 draw offset */
void plat_post_scale(int sx_q8, int sy_q8, int cx, int cy);   /* GBA: BG2PA/PD; PS1: no-op  */
void plat_post_blend(uint8_t color, int level16);   /* GBA: BLDY or palette lerp; PS1: ABR tile */
void plat_post_iris(int cx, int cy, int r, int heart, uint8_t rim);
```

Shake profiles are data (Appendix C): `{frames, xmode, ymode, yc, amp[14]}`,
about 250 bytes for all 14 profiles.

### 7.4 Drawing with squash: one new primitive

```c
/* Scaled blit about an anchor (feet / contact point). sx/sy Q8.8, negative sx = flip.
 * GBA: ARM/IWRAM nearest-neighbour span blit (or an affine OBJ, see §8).
 * PS1: one POLY_FT4. Falls back to plat_image when sx == sy == 256. */
void plat_image_xform(int ax, int ay, const DoomoImage *img, int sx_q8, int sy_q8,
                      int flags, uint8_t color);
```

Callers:

* `ui_portrait()` (talk bob, shout stretch);
* overworld body sprites (hops, landings);
* furniture (drop, pickup, rotate-flip);
* caption bitmaps (POP and SLAM);
* pet LCD sprites.

`ui_heart_squash` already shows the convention.

### 7.5 Audio hooks (`audio.h` additions)

```c
void audio_music_mute(int frames);        /* sequencer keeps running (re-enters on the grid) */
void audio_music_duck(int gain_q8, int frames);
void audio_tapestop(int frames);          /* pitch x 2^(-15/12) ramp over frames, then mute  */
void audio_music_resume_on_beat(void);    /* unmute at the next downbeat / pickup             */
```

They are implemented as a global music gain (the master already multiplies
`OPT_MUSIC_VOL`) and a global pitch multiplier on channel frequencies. That
is a few multiplies per tick.

---

## 8. Cost on GBA and PS1

Budgets per 30 Hz tick: **GBA** 16.78 MHz / 30 = **559k cycles**; **PS1**
33.87 MHz / 30 = **1.13M cycles**, plus the GPU.

| feature | GBA (mode 4, 240x160 8bpp, page-flipped, BG2 affine) | PS1 (320x240, GPU prims, GTE) |
|---|---|---|
| tween / table eval | 10-20 cycles each | same |
| spring step | about 15 cycles per axis (32-bit `mul`) | same |
| beat scheduler | about 50 cycles per tick; scripts in ROM (8 B per event) | same; scripts live in the scene's CD overlay (a cutscene is about 1-2 KB of the ~109 KB slot) |
| **screen shake / punch** | **free**: write `REG_BG2X/BG2Y` in the flip routine (`video_flip`, every 2 vblanks) so it matches the page being shown. The bitmap doesn't wrap, so the edge shows backdrop colour 0. Accept a 2-4 px black border for 3 frames (reads as impact), or pair it with a 1.05x zoom (`BG2PA = 0xF4`) | **free**: GP0 E5 draw offset per frame. To keep the HUD steady, insert a second E5 between world and HUD in the ordering table |
| **snap zoom** | **free**: `BG2PA/PD = 256/scale`, re-centred `BG2X/Y`. Nearest-neighbour chunky pixels; use it for 1-2 frames under the white flash, then cut to a real close-up render (as s05 does) | 3D: change the GTE projection H for free. 2D: cut to the close-up under a flash (no free 2D scale) |
| **white flash / fade** | **free**: `BLDCNT` brighten/darken + `BLDY` 0..16 (matches the trailer's 0..16 levels, and is smoother than its dither) | **free**: one semi-transparent TILE, ABR=1 (add) for flashes, ABR=2 (subtract) for fades. About 77k pixels of fill, well under 1 ms |
| coloured flash (red / lava / vape) | palette lerp of 256 entries times 3 channels (about 5k cycles, under 1%), DMA'd at vblank; 512 B RAM buffer | one additive TILE in that colour, free |
| hellify (cute to hell rot) | **global palette lerp** to a ROM hell palette (512 B ROM), 16 steps, about 5k cycles per step. The trailer's dithered radial creep (s08) would be a per-pixel remap: about 150k cycles, 27% of a tick, so only for a 1 s set piece | global: swap the CLUT / tint. Radial creep: an expanding ring of subtractive semi-transparent polys, cheap |
| iris out/in | **software**: black spans left and right of the circle or heart on each scanline, up to 38k px, about 40k cycles (7%), no RAM. **Or HDMA**: WIN0H/WIN1H per scanline (two windows give the heart's two lobes), about 3k cycles plus 1.3 KB double-buffered table. Given current IWRAM/EWRAM pressure, start with software. The rim is drawn as spans | per-row TILE spans (about 480 small prims, 7.7 KB of prim buffer), or a 4-quad mask texture with a black CLUT (4 prims) |
| squash/stretch blit | ARM/IWRAM scaled span blit, about 6 cycles per destination pixel: a 48x56 portrait at 1.3x (about 4.5k px) costs about 30k cycles (5%). Use it only during the 5-6 frame wobble window; at rest use the fast unscaled blit. **Cheaper alternative:** an **affine OBJ**. OBJ is currently unused (`DISPCNT = MODE4 | BG2`); mode 4 leaves 16 KB of OBJ tiles (512-1023), and `OBJ_PALETTE` already holds `doomo_palette`. A 64x64 8bpp actor is a 4 KB DMA on change, with free scaling and rotation per frame, for up to 4 hero-sized actors (dialogue portrait, held furniture, "!" bangs, caption pops) | **free**: `POLY_FT4` with any quad (also skew and rotation) |
| per-glyph text hop | free: glyphs are redrawn every frame anyway (full-redraw model) | free: one SPRT per glyph already |
| particles (confetti, gibs, hearts) | already in core (`*_fx.c`); stay under about 60 live particles at about 40 cycles each | trivial |
| freeze / hit-stop / panic | free: clock clamping | free |
| audio duck / mute / tape-stop | a gain and pitch multiply in the mixer/PSG update | SPU per-voice pitch and volume registers |
| comedy SFX kit (about 13 sounds) | PSG recipes cost 0 B; PCM about 300-800 B each | ADPCM about 1-4 KB each |

**Isometric housing on GBA.** Furniture is a set of pre-rendered 8bpp iso
sprites, sorted by (x+y) depth and blitted into the mode 4 page.
* Squash goes through `plat_image_xform` on small sprites (16-32 px means
  under 1k px per scaled item, so about 6k cycles).
* The item in your hand is an affine OBJ, so drag, wobble and rotate-flip
  cost no CPU.

**2.5D housing on PS1.** Furniture is a textured quad or low-poly mesh.
Squash scales the vertices around the base anchor before the GTE
transform, which is free.

---

## 9. Accessibility and safety

The timing (holds, gaps, silences) is the joke; the wobble is decoration.
Reduced-motion settings must keep the **timing** and drop only the
**amplitude**:

* **`OPT_UI_BOUNCE`** (0, 1, 2) scales tables toward 1.0 (`amount256` in
  `tw_squash`). At 0, pops become cuts on the same frames.
* **`OPT_SCREEN_SHAKE`** scales the amplitudes. At 0, the shake frames
  become a 1-frame 4/16 darken pulse, so the beat is still marked.
* **`OPT_REDUCE_FLASH`**:
  * flashes are capped at level 6;
  * no red or lava flashes (they become darken pulses);
  * no glitch;
  * the 4-frame hell insert is shown darkened, without strobing.
* **Photosensitivity.**
  * Keep full-screen flashes under 3 per second.
  * The trailer's third-person minigun brightens the whole view on every
    other frame (`s10`, `g_flash = firing && (f & 1)`), a 15 Hz flicker
    for 46 frames. In the game, keep muzzle flash **local** (on the
    weapon or sprite) and never flicker the full screen above 3 Hz.
* **`OPT_SUBTITLES`.** Comic sounds that carry the joke get captions:
  \*squeak\*, \*clap\*, *tink*, (record scratch).

---

## 10. Findings and caveats from the source read

* **The s12 "fence squeezes in" never moves.** `squish` is declared
  `1.0f` (`s12_census.c:454`) and never written. The cork pop sells the
  gag on its own. If the game reuses the idea, animate the squeeze, for
  example `squish = 1 - 0.12*smooth((f-100)/8)` with `TW_JELLY` on the
  crowd.
* **`tk_bounce` never reaches exactly 1.** It ends at 0.986, and every
  caller snaps to 1.0 after N frames. That 1.4% snap is invisible at 240 px
  but would show on large PS1 sprites. Clamp the LUT's last entries to 256
  in the game.
* **The trailer's pops are 2-3x faster than the game's UI pop.**
  `ui_pop_scale()` (`ui_draw.c:189`) takes 12 ticks, peaking at 1.17 at
  tick 5. The trailer's comic pops take 4-6 frames. Keep 12 for menus
  (readability) and use the 4-6 frame tables for comic bubbles, captions
  and reactions.
* **`music.c` duplicates scene frame constants by hand.** That is the
  `FR(n)` coupling. The beat scheduler's named anchors are the fix.
* **The game's `SFX_*` list (`audio.h`) has no comedy kit yet** (see §4).
* **The game's cutscenes are `Panel` lists with `min_t` only**
  (`cutscene.c`). They have no beats, sounds or camera, so §7.2 is the
  migration path.

---

## Appendix A: trailer keyframe tables in Q8.8 (256 = 1.0)

```c
/* POP (appear) */
static const int16_t TW_POP_BUBBLE[]  = { 102, 307, 236, 266, 256 };          /* s04 bubble   */
static const int16_t TW_POP_LETTER[]  = { 102, 333, 230, 269, 256 };          /* s02 subtitle */
static const int16_t TW_POP_SOUL[]    = {  77, 333, 236, 269, 256 };          /* s11, s14     */
static const int16_t TW_POP_LOCKUP[]  = {  51, 346, 230, 271, 251, 256 };     /* s12          */
static const int16_t TW_POP_EYE[]     = {  77, 346, 218, 282, 256 };          /* s05          */
static const int16_t TW_POP_MENU[]    = {  38, 141, 287, 243, 264, 256 };     /* s11          */
static const int16_t TW_POP_BLOB[]    = {  51, 320, 230, 269, 256 };          /* s15          */
static const int16_t TW_POP_BURST[]   = {  77, 205, 410, 486, 410, 410 };     /* s06 (ends 1.6x) */
/* SLAM (land on screen) */
static const int16_t TW_SLAM_STAMP[]   = { 614, 410, 236, 266, 256 };         /* s11          */
static const int16_t TW_SLAM_COUNTER[] = { 563, 384, 218, 282, 248, 256 };    /* s11 (x1.2 on #3) */
static const int16_t TW_SLAM_STICKER[] = { 410, 320, 236, 266, 256 };         /* s02          */
static const int16_t TW_SLAM_STICK2[]  = { 461, 333, 243, 256 };              /* s02          */
/* DROP impact (squash) */
static const int16_t TW_DROP_TITLE_SX[] = { 353, 307, 230, 246, 264, 256 };   /* s02/s15      */
static const int16_t TW_DROP_TITLE_SY[] = { 159, 205, 287, 269, 251, 256 };
static const int16_t TW_DROP_TITLE_DY[] = {   0,   0,  -4,  -4,  -1,   0 };   /* px           */
static const int16_t TW_DROP_LET_SX[]   = { 333, 287, 241, 264, 256 };        /* s01          */
static const int16_t TW_DROP_LET_SY[]   = { 184, 230, 276, 251, 256 };
static const int16_t TW_DROP_LET_DY[]   = {   0,   0,  -3,  -1,   0 };        /* px           */
static const int16_t TW_DROP_SOFT_SX[]  = { 312, 282, 243, 251, 261, 256 };   /* s14 handheld */
static const int16_t TW_DROP_SOFT_SY[]  = { 200, 230, 274, 264, 253, 256 };
static const int16_t TW_DROP_SOFT_DY[]  = {   6,   3,  -3,  -2,   0,   0 };   /* px           */
static const int16_t TW_HEART_LAND_SQ[] = {  87,  61,  20 };                  /* s01 sq (+ = wide); w*(1+sq), h*(1-0.9sq) */
static const int16_t TW_DEVICE_DY[]     = {   5,   3,  -3,  -3,  -1,   1,   0 }; /* s04 px     */
static const int16_t TW_SIGN_DY[]       = { -14,  -6,   2,   0,  -1,   0 };   /* s12 px       */
static const int16_t TW_CLUNK[]         = { 210, 236, 271, 261, 256 };        /* s14 console sy */
static const int16_t TW_CLAP[]          = { 230, 241, 266, 261, 256 };        /* s14 sy (sx = 512 - sy) */
/* motion offsets (px) */
static const int16_t TW_WINDOW_BOING_DY[] = { 160, 120, 70, 28, -2, -9, -6, 1, 3, 1, 0 }; /* s05 */
static const int16_t TW_BRAKE_DX[]      = { 6, 8, 6, 2, -1, -2, -1, 0 };      /* s03          */
static const int16_t TW_ZOOM_WOBBLE_DY[] = { 6, -3, 1, 0 };                   /* s05          */
static const int16_t TW_HOP2[]          = { -7, -4, 1, 2, 1, 0 };             /* bcap, scale 2 */
static const int16_t TW_HOP1[]          = { -3, -2, 0, 1, 0, 0 };             /* bcap, scale 1 */
static const int16_t TW_TAG_DY[]        = { -3, 1, 0 };                       /* "(cozy.)"    */
static const int16_t TW_DEADPAN_DY[]    = { 8, 6, 4, 2, 0 };                  /* s07 slide-up */
```

## Appendix B: the `tk_bounce` LUT in fix16 (t = i/32)

```c
static const int32_t TW_BOUNCE_FX[33] = {
    0, 9069, 18637, 28148, 37179, 45425, 52689, 58863, 63916, 67874, 70809, 72821, 74033,
    74573, 74572, 74155, 73439, 72524, 71500, 70439, 69398, 68422, 67539, 66768, 66121,
    65596, 65191, 64897, 64700, 64588, 64545, 64559, 64614 };   /* clamp to 65536 at i>=30 in game */
fix16 tw_bounce(fix16 t) { if (t <= 0) return 0; if (t >= FIX_ONE) return FIX_ONE;
    int i = t >> 11, fr = t & 0x7FF;                     /* 32 segments                      */
    return TW_BOUNCE_FX[i] + (((TW_BOUNCE_FX[i + 1] - TW_BOUNCE_FX[i]) * fr) >> 11); }
```

## Appendix C: shake profiles as data

```c
enum { SX_ALT, SX_RAW, SX_NONE };            /* x = (f&1) ? +amp : -amp | amp as signed | 0      */
enum { SY_NONE, SY_HALF, SY_CONST, SY_RAW }; /* y = 0 | (f&2) ? +-amp/2 | (f&2) ? +-yc | amp signed */
typedef struct { uint8_t frames, xmode, ymode; int8_t yc; int8_t amp[14]; } ShakeProf;
static const ShakeProf SHAKE[SHK_COUNT] = {        /* source (trailer)                      */
  [SHK_TAP]     = {  2, SX_NONE, SY_RAW,   0, { 3, -1 } },             /* s12, s02 (2,-1), s14  */
  [SHK_BRAKE]   = {  3, SX_RAW,  SY_NONE,  0, { 1, -2, 1 } },          /* s03 record scratch    */
  [SHK_POW]     = {  3, SX_ALT,  SY_CONST, 1, { 2, 2, 2 } },           /* s06 POW, s14 clunk    */
  [SHK_IMPACT]  = {  3, SX_ALT,  SY_CONST, 2, { 3, 3, 3 } },           /* s02 splat, s04 insert */
  [SHK_STAMP]   = {  3, SX_ALT,  SY_CONST, 2, { 3, 2, 1 } },           /* s11 fail x1/x2, LOST  */
  [SHK_STAMP3]  = {  5, SX_ALT,  SY_CONST, 2, { 3, 3, 2, 2, 1 } },     /* s11 fail x3           */
  [SHK_SLAM]    = {  5, SX_ALT,  SY_HALF,  0, { 3, 3, 3, 3, 3 } },     /* s08 caption slams     */
  [SHK_IGNITE]  = {  5, SX_ALT,  SY_CONST, 2, { 3, 3, 3, 3, 3 } },     /* s15, s11 monster (4)  */
  [SHK_SHOTGUN] = {  7, SX_ALT,  SY_HALF,  0, { 7, 6, 5, 4, 3, 2, 1 } },
  [SHK_FWOOMP]  = {  8, SX_ALT,  SY_CONST, 2, { 4, 4, 3, 3, 2, 2, 1, 1 } },
  [SHK_CRACK]   = {  8, SX_ALT,  SY_HALF,  0, { 3, 3, 3, 3, 3, 3, 3, 3 } },   /* + glitch 22-2a */
  [SHK_RUMBLE]  = { 12, SX_ALT,  SY_CONST, 2, { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 } },
  [SHK_ROAR]    = { 14, SX_ALT,  SY_HALF,  0, { 4, 4, 4, 4, 3, 3, 3, 3, 2, 2, 2, 2, 1, 1 } },
  [SHK_GUNFIRE] = {  1, SX_ALT,  SY_CONST, 1, { 1 } },                 /* repeat while firing   */
};
/* PS1: scale amp by 4/3 (x) and 3/2 (y); OPT_SCREEN_SHAKE scales all amps (0 -> darken pulse). */
```

## Appendix D: reproducing these numbers

All of the following live in the scratchpad (`story/`). Nothing was
written to the repo. Access was read-only: `cat`, `grep` and `sed`, plus one
final `git status` check. That check can refresh the index's stat cache but
does not change tracked content.

* `story/trailer_build/`: a copy of `trailer/` built with
  `gcc ... -I<repo>/core/include` into `trailer_build/out/trailer`.
* `story/frames/*.png`: contact strips of the key beats, with frame numbers
  printed on each cell:
  * `s01_land`, `s01_tail`, `s02_splat`, `s04_evil`, `s05_zoom`,
    `s07_fwoomp`, `s08_scratch`, `s11_three`.
  * The tool is `story/strip.py <frames.rgb> <out.png> <global_start>
    <count> <step> <cols> [label_offset]`.
  * Regenerate the input with `trailer_build/out/trailer --raw > frames.rgb`.
    It is about 270 MB, so it was deleted after use.
  * Global scene offsets in frames: s01 0, s02 75, s03 195, s04 345, s05
    510, s06 660, s07 810, s08 975, s09 1155, s10 1290, s11 1545, s12
    1770, s13 1890, s14 2040, s15 2175.
* `story/tools/analyze.py`: the `tk_bounce` analysis, curve fits, area
  ratios and the silence map.
* `story/tools/springs.py`: the spring presets, an integer Q4/Q14
  verification, and the Q8.8 table conversion.
* `story/tools/ctest/t.c`: compiles the Appendix B LUT, `tw_spring_step`
  and `tw_tab` with `-std=c99 -Wall -Wextra` and checks them against the
  float originals.
