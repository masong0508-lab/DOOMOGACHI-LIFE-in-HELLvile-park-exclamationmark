# DOOMOGACHI LIFE in HELLvile Park! - Music Direction and Cue Sheet

> **File:** `scratchpad/music/direction.md`. This is a design spec, not code.
> It was written read-only against the repo on 2026-10-11.
>
> **Read for this spec:** `docs/GAME_DESIGN.md`, `docs/STORY.md`,
> `docs/STUDIO.md`, `docs/GBA.md` (sections 3 and 7, the RAM and fps tables),
> `docs/PS1.md` (RAM, SPU RAM, audio, performance), `core/include/doomo/{scenes,
> activities,audio,song,map,story,platform,catalog}.h`, `core/src/{songs,
> audio_int.h,levels.c,levels_int.h,concert_int.h,dream.c,mg.h,fishcook_int.h,
> credits.c,story_places.c,overworld.c,cutscene.c}`, `gba/src/{irq.iwram.c,
> audio_int.h}`, `trailer/music.c`, `trailer/README.md`, and
> `scratchpad/custom/wishlist.md` section 8.17 (audio options).
>
> **Story spec:** when this was written, `scratchpad/story/` held only build
> artefacts and no design doc. The story hooks below are written against the
> existing beats (`BEAT_*`, `CUT_*`, `PLACE_*` and the region names) and the
> planned additions named in the brief: deeper chapters and episodes, 80+ side
> missions, the Dream House (GBA isometric, PS1 2.5D) and a comic-timing
> engine borrowed from the trailer. Section 11 lists what to reconcile when
> that spec lands.

---

## 0. The pitch in twelve lines

1. **Super chiptune.** Every sound is a pulse, triangle, saw, noise,
   wavetable or crunchy 8-bit sample, played with chip gestures: arpeggio
   chords, slides, sweeps, delayed vibrato and echo-by-repeat. The consoles
   are pushed by doing **more chip at once**, never by leaving chip behind.
2. **Two themes share one cell.** HaHaland's **Hook** and Hellvile's **Riff**
   open on the same three pitches, **E-E-G**. In C major that cell is sunny
   (mi-mi-sol). In E minor it is the tonic of a metal riff (1-1-♭3). The Wall
   between the districts is a reharmonisation, and the ending takes it down.
3. **Same melody, different band.** A five-step **style dial** (music box →
   toy pop → spooky-cute → chip rock → chip metal) morphs any cue at a bar
   line. The invasion is a turn of the dial, not a new song.
4. **Never the same 8 bars twice in 10 minutes.** The tools: phrase graphs
   with weighted no-repeat walks, per-pass variation, reharmonisation pools,
   generative drums with humanised velocity, stems by intensity, key and
   tempo journeys, motif quotes, ambient beds and planned rests.
5. **Comedy is timing.** The music engine serves the comic-timing engine. It
   lands stings on frames, drops out for deadpan beats, and scratches the
   record for "wait, WHAT?".
6. **Record scratches are real DSP on both consoles.** DECK scrubs the
   sequencer itself back and forth, TAPE plays back a ring buffer of the real
   recent mix, and synthesised vinyl crackle and friction ride on top. The
   scratches serve as comic stings, music transitions and player-placed DJ
   moments in the Studio and Concert Hall.
7. **GBA: every channel in use.** All 4 PSG channels (sweeps, duty switching,
   dual-bank wave morphing, LFSR modes) plus a stereo DirectSound soft synth
   in IWRAM ARM code: PWM, sync saw, wavetables, PCM drums, vocal chops, echo
   and the scratch ring. It adds 2 to 11 music voices by scene tier (up to 16
   chip voices fit the 20 % budget) and costs the same CPU as today in 3D
   scenes.
8. **PS1: the whole SPU.** All 24 voices, ADPCM chip-wave banks (16 duties,
   morph loops, baked FM), hardware PMON frequency modulation, the NON noise
   generator, ADSR, volume sweeps for auto-pan, a reverb preset per area, and
   the score library kept in SPU RAM so main RAM stays free.
9. **Deterministic.** The arranger is core C (no floats, no malloc), seeded
   from town + day + cue. The GBA and the PS1 play the same notes. Tiers only
   drop *rendering* of low-priority parts, never *composition*.
10. **The existing work survives.** The 19 existing songs remain valid; they
    become the A-phrases of the new cues. The trailer's hook and riff are
    canon.
11. **Music breathes.** Planned rests, ambient beds and silence-as-a-cue keep
    a 3-hour story and endless free play from wearing out the ear.
12. **First-class on both consoles.** Every feature has a GBA rendering and a
    PS1 rendering (section 9). Neither console gets an exclusive.

---

## 1. Sound identity

### 1.1 The palette rules

| Allowed (the chip vocabulary) | Not allowed |
|---|---|
| Pulse at 12.5 / 25 / 50 / 75 % and true PWM; triangle (smooth and 4-bit stepped NES-style); saw and hard-sync saw; long and short LFSR noise; 4-bit and 8-bit wavetables (single cycle and morphing multi-cycle); baked or live 2-op FM that still sounds like a chip | orchestral samples, realistic guitar or piano samples, real recorded vocals, CD-DA or XA music streams, long lush reverbs on the GBA |
| Chip gestures: 3-4 note arpeggio chords at 50/60 Hz, pitch slides, hardware sweeps, delayed vibrato, duty sweeps, echo-by-repeat (a quieter delayed copy of a line on another channel), octave-hopping bass, noise drum fills | anything that reads as "a real band in a room" |
| 8-bit PCM for drums and vocal chops at 8-16 kHz: short, crunchy and quantised, as if it came out of a chip | samples longer than about 1 s except loops |
| Singing is **babble** (resident voices, `audio_voice_char`), vowel-formant wave cycles and "aah/ooh" chip-choir loops | words |

**Chip metal is still chip.** Power chords are a saw or sync-saw pair, or a
single wave-channel cycle that holds root + fifth. Palm muting is a short
gate (`q3`, as in "Hell Invades"). The double kick is a PCM kick in 16ths.
The whammy is 2-3 semitones of vibrato depth. Twin guitars are two pulses a
third apart, panned left and right.

### 1.2 The style dial: one melody, five bands (plus three specials)

Every cue can be rendered in any of these styles. A cue declares its home
style and the range it may move through. Section 5.7 covers how a cue
morphs between them.

| Dial | Name | Lead | Harmony | Bass | Drums | Groove |
|---|---|---|---|---|---|---|
| **S0** | Music box | 12.5 % pulse, fast decay, tiny echo | celesta arps (8th-note broken chords) | soft triangle, roots | none, or a ticking clock | rubato-feeling, 3/4 or 4/4 |
| **S1** | Toy pop (HaHaland) | 25 % pulse, delayed vibrato | major arp chords (`@4`), off-beat stabs | triangle roots and fifths | **Toy Box** kit: tiny kick, clap, shaker hats | straight or light swing (0-2) |
| **S2** | Spooky-cute (Hellvile) | 12.5 % "wobble" pulse, deep vibrato | harmonic-minor arps, dim chords | wave-channel "organ" cycle | **Bone Rattle** kit: rim, claps, bone clicks | swing or shuffle (5-6) |
| **S3** | Chip rock | 25 % pulse with duty sweep, or sync saw | power-chord wave (one channel) | saw bass | **Classic** / **Boom 808** kit | straight, 8ths |
| **S4** | Chip metal (Hell) | sync saw, whammy vibrato, twin harmony | power chords doubled L and R ("wall of saw") | saw + sub octave, palm-muted chugs | **Hell Forge** kit: PCM double kick, noise snare and crash, china | 16th double kick, blast bursts, half-time breakdowns |
| *L* | Lounge (Vex) | breathy 50 % pulse, slow vibrato, portamento | maj7/m9 PWM pads | walking triangle | brushes (long noise), finger snaps | swing 4 |
| *H* | Hymn (Fuzz) | 50 % + 25 % stacked "pump organ" | 4-part organ | organ pedal | none, or one bell | free / 72 bpm |
| *P* | Piezo (pet LCD) | one or two 50 % beeps, no envelope | none | none | none | strict |

### 1.3 Key and tempo geography

- **Hillvile / HaHaland** (the good side): C, F, G and D major, 104-144 bpm,
  with add9 and maj7 sparkle. The Hook lives in **C**.
- **Hellvile** (the people of the bad side): **E minor** (harmonic), D minor
  for B sections, 92-112 bpm, swung.
- **Hell** (the invasion and the Hell Breaches): E minor and E phrygian, with
  **F minor** for climaxes. 150-200 bpm.
- **The Park** (the ending): **E major.** E is the 3rd of C (Hillvile) *and*
  the root of E minor (Hellvile), so the park's key is the note both sides
  already share. The last chorus goes up a semitone to **F major**, the
  "truck driver" lift.
- Fixed homes: church F major, Vex's lounge A minor, the Doomogachi C major
  (LCD), the Dream House C major in 3/4, the credits E→F.
- **Tempo ladder:** 60-84 for sad, night and lullabies; 88-112 for the
  lounge, Hellvile and home; 116-144 for HaHaland and fun; 150-184 for
  combat; 200+ for Monster Mode Mayhem only.

### 1.4 Mix aesthetics

- **Stereo by role.** The lead sits in the centre; twin leads go L and R;
  arps ping-pong; hats sit slightly right; the power-chord stack is wide;
  bass and kick stay centred.
- **GBA speaker vs headphones** (wishlist AU10). The original GBA speaker is
  mono and has no low end. *Speaker* mode folds the mix to mono-compatible
  and adds the 2nd harmonic to the bass (a different wave-channel cycle and
  a soft octave double), so the bass is still audible. *Headphones* mode
  keeps the full stereo width and the sub octave.
- **Space.** The PS1 uses a hardware reverb preset per area (section 8.4).
  The GBA has no reverb, so rooms are faked with the soft echo (tier S) and
  with echo-by-repeat everywhere.
- **Ducking.** Under dialogue the lead stem mutes and the rest drops 6 dB:
  the melody hands the microphone to whoever is talking. The music pumps
  under big SFX (the chiptune sidechain) and ducks under stingers.
- **Babble in tune.** Babble syllable pitches snap to the current chord's
  tones, so citizens sound like they sing along with the music (the voice
  sliders still pick the register and contour). Both consoles do this. With
  more voices available, babble gets its own voice and no longer replaces
  the harmony line.

---

## 2. Leitmotifs

Notation is the repo's MML dialect (`audio_int.h`): `o` octave, `<` `>`,
`l` default length, `+` sharp, `-` flat, `r` rest. Scale degrees are given
in the motif's reference key. Every motif has a **head** (2-4 notes that
are always quoted literally, for recognition) and a **tail** (which the
variation engine may bend).

### 2.0 The big idea: the shared cell E-E-G

```
HOOK  (C major)  o5 l8  e  e  g  e >c4< g4      mi  mi  sol  mi  do'  sol
RIFF  (E minor)  o2 l16 e  e  g8 e  e  b-8 ...  1   1   b3 | 1   1   b5 ...
                        ^^^^^^^^
                 the same pitches, E E G: a different side of the same wall
```

- The **Hook** hears E-E-G as the bright 3rd and 5th of C, then leaps up to
  C: an open, friendly gesture.
- The **Riff** hears E-E-G as tonic and minor third, then pushes to B♭, the
  tritone (*diabolus in musica*), the "hell" interval.
- The **Wall** motif (the existing "The Wall" cue, E-F | E-B♭) is that
  tritone exposed and alone.
- **Redemption** is a two-note edit. In the ending the Riff's G becomes G♯
  and its B♭ becomes B: the tritone resolves up to the perfect fifth, and
  the Riff turns major. Rivet, the runaway imp, is the first character whose
  motif uses the redeemed form (section 2.7), so the ending is foreshadowed
  from Chapter 2.
- **The finale** stacks both themes in E major (section 2.9). Because they
  start on the same notes, they do not merely coexist; they *agree* on the
  first beat.

### 2.1 HOOK: HaHaland / Hillvile ("Make friends!")

Canon from the trailer (`HOOK1/2/3/7` in `trailer/music.c`) and close to the
existing Title cue.

```
t140 o5 l8
| e e g e >c4< g4 |        C    "Make friends!"
| a a g e  d4  e4 |        Am
| f f a f >c4< a b |       F  G  "Fall in love!"  (bar 3, alt A; this is LOVE, 2.10)
| f a >c< a g f e d |      F  G  (bar 3, alt B: the descending run)
| >c2< r4 r8 g |           C     (bar 4, alt A: full cadence + pickup)
| g2 r4 g8 g8 |            G     (bar 4, alt B: half cadence, stays open)
| e4 g4 >c4< r4 |          C     (bar 4, alt C: bright button)
```

- **Degrees:** 3 3 5 3 8 5 | 6 6 5 3 2 3. **Head:** E-E-G-C'. **Rhythm cell:**
  four 8ths and two quarters (short-short-short-short-long-long).
- **Harmony:** I - vi - IV - V (C Am F G). The phrase graph (5.1) swaps bars
  3 and 4 between the alternatives above, so even the main theme has 6
  spellings.
- **Forms:** S0 music box (credits epilogue, title idle); S1 the HaHaland
  Day lead; S2 in E minor with a ♭6 for Hellvile visits; S3 and S4 in E major
  as the metal anthem (trailer s13); the lounge form (Vex "covers" it at the
  vape opening, its last note sliding down a semitone); the "infected" form
  (5.11), where the tail droops chromatically.

### 2.2 RIFF: Hellvile ("The Other Side")

Canon from the trailer (`RIFF_A1/A2`) and the existing "Hell Invades" and
"Hell Breach".

```
o2 l16
| e e g8  e e b-8  e e a8  e e g f+ |            Em   (bar 1: "the chug")
| e e g8  e e b-8  e e >c8< b8  b- a |           Em   (bar 2: "the drip", C B Bb A chromatic)
```

- **Degrees:** 1 1 ♭3 | 1 1 ♭5 | 1 1 4 | 1 1 ♭3 2, then the chromatic drip
  ♭6 5 ♭5 4. **Head:** E-E-G-B♭.
- **Three lives**, which double as the story arc:
  1. **Hellvile folk** (S2): the Riff as a grumbly, lovable swing tune at
     108 bpm on a wobble pulse with claps. It is the song of the citizens.
     *Retune "Hellvile After Dark" from D minor to E minor* so the shared
     cell is literal; keep D minor for its B section.
  2. **Hell's war march** (S4): Lord Vapula weaponises the citizens' song
     into the invasion metal (168-184 bpm, double kick).
  3. **Redeemed** (S1-S4): `e e g+8 e e b8 e e a8 e e g+ f+` and in bar 2
     `>c+8< b8 a g+`. It is major and has a fifth instead of a tritone.

### 2.3 BEEP: the player's Doomogachi

The trailer's running gag is a single LCD **"beep."**, so the motif is
built from beeps and **grows with the pet**.

```
reference C, piezo 50 % pulse, o6 l16
Egg    : >c<                                      one beep.
Baby   : c >c<                                    two
Child  : c g >c8<                                 three
Teen   : c e r g >c8<                             four, syncopated
Adult  : c e g r >c8< r8 ... (1 beat rest) ... >c32<   the full call + the "beep." tag
```

| Care line | Adult form | Colour |
|---|---|---|
| CUTE (good care) | `c e g r >c<` | major, bright |
| ANGEL (holy) | `c g >e< r >b<` | open maj7, harp-like (S0 bell) |
| FERAL (neglect) | `c c+ g r >c<`, displaced by a 16th | crunchy ♭2, scratchy noise beat |
| DEMON (corrupted) | `c e- g- r >c<`, and the "beep." tag drops a tritone to G♭ ("bweep.") | diminished |

- The motif is quoted as **descant** whenever the pet follows the player
  (5.11), as the "NEW UNLOCK!" sting (adult form, current line), and as the
  top line of the finale.
- The 16 pet forms colour it further by line × stage. Register drops one
  octave from Child to Adult, and tempo eases as the pet ages.

### 2.4 VEX: "the exhale" (too smooth)

```
reference A minor, lounge, swung, slurred: o4 a2  >e-4 d8 d-8 | c2.< r4 |
```

- **Shape:** a rising tritone, then a chromatic slide down, like smoke curling
  out of a mouth. **Every interval is a portamento glide** (MML extension `j`,
  section 10); the slides *are* the vape drag.
- **Harmony:** Am9 - Fmaj7 - Dm9 - E7♭9: City Pop lounge, too pretty to trust.
- **The reveal is built in.** Vex's motif is Lord Vapula's motif (2.5) in
  disguise: the same contour, transposed, swung and slurred. Players hear
  the family resemblance before the story says it (the trailer already makes
  the crowned cloud "employee of the month" at Cloud Nine Vapor). Play Vex
  straight, slow and on power chords, and Vapula appears.
- In the credits Vex "now sells plain water": the motif comes back in A
  **major** with the tritone straightened to a fifth (A-E-D-C♯-C♯).

### 2.5 VAPULA: "rise of the tritone, fall of the smoke"

```
reference E minor, o4: e2 b-4 a8 a-8 | g2. r4 |
```

- **Shape:** the tonic leaps up a tritone, then falls chromatically to G.
  That G is the Riff's minor third, so Vapula's line lands on the citizens'
  own cell: he has hijacked their song.
- **Forms:** phase 1 in augmentation (half-time doom); phase 2 in diminution
  (16ths, twin harmonised); phase 3 **inverted** (`e2 b-4 b8 >c8 | c+2.<`,
  rising chromatic smoke climbing to the Ninth Cloud); death **deflates**
  (a tape-stop down two octaves, section 6). The credits give him a
  pocket-sized tuba-pulse cameo, staccato.

### 2.6 FATHER FUZZ: "the plagal hug"

```
reference F major, hymn H: o5 l4 f a g f | b- a8 g8 a2 |   (head of the existing "St. Cuddles Hymn")
```

- **Head:** F-A-G-F (1 3 2 1), a little turn that comes home like a hug.
- **Signature:** every Fuzz cue ends on a **plagal "amen"** (IV - I, B♭ → F).
  An exorcism success is the biggest amen in the game.
- **MOTHER NOX** (the High Priestess of Hellvile) gets the **mirror image**:
  `f d- e- f` in F minor (the inversion of 1 3 2 1). Kind, not evil. At the
  festival the two hymns play as a **canon in contrary motion**.
- **Bishop Bonebottom's anti-hymn** (Bone Cathedral) is Fuzz's head in F
  phrygian (`f a- g- f`, ♭3 and ♭2) over a tritone pedal (B), on a pipe
  organ and in metal.
  "LET US PRAY... FOR YOU!"

### 2.7 RIVET: "the redeemed riff, heard first"

```
reference G mixolydian funk (the key of "Rivet's Workshop"), o3 l16:
| g g b8  g g >d8<  g g >c8<  g g b a |   + an anvil "tink" on beat 4
```

- This is the Riff **redeemed** (major 3rd, perfect 5th) on G. Rivet ran
  away from Hellvile and is good. She is the first character to play the
  ending's harmony, in Chapter 2.
- **Band:** slap-triangle bass (`@18`), clanky 7-bit-noise "anvil" and PCM
  metal hits, a gallop on the hats.
- **Forms:** "Rivet goes home" in Chapter 4 is her motif in Hellvile's swing,
  sentimental, then joined by Anvil's Forge family playing the Riff:
  father's minor version and daughter's major version, in thirds.

### 2.8 HOME: the Dream House ("the Hook, coming back to you")

```
reference C major, 3/4, 84 bpm, music box S0:
| g4 >c4 e4 | g2 e4 | e2. |        (bar 1 of the Hook in retrograde: g c' e g e e)
answer: | d4 e4 g4 | >c2.< |
```

- **Shape:** a rising arpeggio, then a sigh. It doubles as the **doorbell**
  (the first three notes), the **save jingle** (the full phrase) and the
  lullaby.
- **House states** (driven by `comfort`, `spook` and `fort`, section 3):
  - **Cosy:** a waltz in C; harp arps join as comfort rises; the radio in
    the living room plays whatever the jukebox holds, low-passed, with
    crackle.
  - **Haunted:** `g >c e- | g2 e-4 | d2.` in C minor. The resolution never
    arrives, the music box is detuned, and the WOBBLE warped-record scratch
    pattern runs on it (section 6). Ghost "ooh" choir.
  - **Barricaded:** the motif in 4/4 as a tom-tom and low-pulse march
    (`o3 l8 g g >c c e e g4`), with hammering SFX on the beat and the style
    dial at S2-S3. When the invasion is at the door the march meets the
    Riff.
  - **Party:** disco 4/4 (S1 with an octave bass and open hats).

### 2.9 COMMINGLING: the finale counterpoint

Key E major, 144 bpm (the festival), the chords of the Hook (E | C♯m | A B |
E). Four layers that agree on beat 1, because the Hook and the Riff share
their first notes:

```
            | E                   | C#m                 | A        B          | E                  |
DESCANT     | r1                  | r2.  (BEEP adult)   | r1                  | r2 >e16 g+16 b16 r16 >e8< r8 |  ("beep.")
HOOK        | g+ g+ b g+ >e4< b4  | >c+ c+< b g+ f+4 g+4| a a >c+< a >e4 c+ d+<| >e2< r2            |  (o5 l8)
HYMN canon  | e4 g+4 f+4 e4       | c+4 e4 d+4 c+4      | a4 >c+4< b4 a4      | b4 a4 g+2          |  (Fuzz head, o4; Nox's
            |                     |                     |                     |                    |   mirror enters 2 bars later)
RIFF (red.) | e e g+8 e e b8 e e a8 e e g+ f+ | c+ c+ e8 c+ c+ g+8 c+ c+ f+8 c+ c+ e d+ | a a >c+8< a a >e8< b b >d+8< b b f+ d+ | e e g+8 e e b8 e e >c+8< b8 a g+ |  (o2 l16)
```

- **Section plan for the festival and credits cue** (section 4.19):
  A = Hook over the redeemed Riff; A' = adds the Riff's original minor drip
  as a call-and-response ("Hillvile calls, Hellvile answers"); B = HOME in
  4/4 augmentation as the bridge ("everyone goes home together"); C = the
  hymn canon (Fuzz and Nox) over a pedal E; cameos (Vex in major, Vapula
  deflated, Rivet's anvil); the final chorus +1 semitone to F major with
  every layer stacked; the end is Fuzz's amen (IV - I) and a single "beep.".

### 2.10 Secondary motifs

| Motif | Reference | Shape | Where |
|---|---|---|---|
| **LOVE** ("Fall in love!") | C: `f f a f >c4< a b` → resolves to C | bar 3 of the Hook; it ends on the leading tone (longing that resolves) | romance, confessions, weddings, matchmaking quests, cross-border couples (in counterpoint with the Riff) |
| **MAYOR** (Gigglesworth) | C: `c12 c12 c12 e4 g4 >c4<` | the existing Victory head: a civic triplet fanfare | Town Hall, quest complete, "Mayor crosses over" (in E minor → E major) |
| **NANA** (Pudding, baker) | F: `c8 f8 f8 a8 g4 f4` | a bouncy polka pickup | bakery, the egg gift, Cupcake Catch |
| **PIP and Mr. Flops** | G: `d8 g8 b16 a16 g8 d4` + a squeak | a kid's skipping tune | the lost bunny quest, the credits |
| **Dr. BLOOM** (ghoul doctor) | A minor tango: `a8. b16 >c8< b8 a4` + heart-monitor beeps on the beat | "cold hands, warm heart" | the hospital |
| **WALL** | E: `e2 f2 \| e2 b-2` | semitone + tritone: dread | the Wall area in Ch.1-2, the fog, the "dun dun dunn" sting |
| **TRUE NAME** (each Hellvile citizen) | 4 notes generated from the resident id (degrees from E harmonic minor, rhythm from their catchphrase's syllable count) | a little "signature" per citizen | exorcism phase 3, their apartment window, their credits pass |
| **GOODBYE** | A minor: the existing "Goodbye, Friend" head `e2 d4 c4 \| <b2 a2>` | the falling 3rd-2nd-1st steps | sad, souls lost, laying to rest, death |
| **Mini-boss motifs** | see 4.8 | | Hell Breach 1, 3-7 |

### 2.11 Motif transformation matrix

How each motif is rendered at each point of the dial (5.7 explains the
mechanics). "Redeemed" means the major-third and perfect-fifth spelling.

| Motif | S0 music box | S1 toy pop | S2 spooky-cute | S3 chip rock | S4 chip metal | Lounge / hymn / piezo |
|---|---|---|---|---|---|---|
| HOOK | lullaby, C | HaHaland Day lead | E minor with ♭6, swing | E major anthem | E major + power chords (trailer s13) | Vex's cover, the droop (Ch.2) |
| RIFF | detuned music box (nightmares) | redeemed, toy kit | Hellvile folk swing | rock riff | war march | busker accordion-pulse (Imp Market) |
| BEEP | angel-line lullaby | the pet song | feral/demon lines | Rivet-tuned "robot beep" | 4-frame hell glitch | **native: piezo** |
| VEX/VAPULA | the haunted music box | Vex's water stand (credits) | Vex at the vape opening | Spire lift muzak | Vapula boss | Vex lounge (native) |
| FUZZ/NOX | church at night | festival hymn pop | Nox's chapel | - | Bonebottom's anti-hymn | **native: hymn** |
| RIVET | anvil lullaby | workshop funk | Forge Row swing | weld game | Gallery Hellfire tier | - |
| HOME | cosy (native) | party | haunted | barricaded march | the house under siege | radio |
| LOVE | proposal | confession accepted | cross-border crush | wedding party | - | wedding organ |

---

## 3. Music state: the drivers

All drivers are `uint8_t` 0..255 unless noted, smoothed with an attack
(rise) and release (fall) in logic ticks so the music never flaps. Stem
changes happen at bar lines with a **minimum dwell of 4 bars**. The one
exception is a danger spike, which may jump to I4 on the next beat behind a
fill.

| Driver | Source in today's code (or the planned module) | Attack / release | What it drives |
|---|---|---|---|
| `danger` | combat: actors in CHASE/ATTACK within a radius, weighted by 1/distance; boss alive | 0.3 s / 6 s | intensity I2→I5, drum density, crash accents |
| `enemies` (0..15) | the count of awake demons near the player | 0.5 s / 4 s | how many METAL stems play |
| `hp` | player health fraction | instant / 2 s | below 25 %: a heartbeat stem (kick at the beat, 70 bpm feel over the tempo), lead drops out, highs muffled (PS1 muffled banks, GBA soft LP + PSG duty → 50 %) |
| `combo` | kill streak / style meter | instant / 3 s | lead ornaments, fills, crash on every downbeat at 200+ |
| `boss_phase` (0..3), `boss_hp` | `lv_run`, the boss actors | event | phase sections, tempo +4 per phase, key up in phase 3 |
| `hell` | the level hell blend (`lv_info.hell_at`), invaded map, gore | 1 s / 1 s | **the style dial** S1→S4 |
| `infect` | Chapter 2 progression (C2_VAPOR..CELLAR) + possessed count + nearness to Cloud Nine Vapor | 4 s / 20 s | Vex droops on the Hook's tail, detuned harmony, a ♭5 pedal at 192+ (5.11) |
| `hour` (0..23 + minutes) | `town_hour()`, the town clock | continuous | time-of-day variant, tempo curve (4.2) |
| `weather` | `fp_island_weather()`: WX_CLEAR / RAIN / SNOW / PETALS (+ the story's own mood particles) | change at a phrase | drum kit swap, harmony pool, ambient bed |
| `season` | `sim_date()` month | daily | colour (spring sus chords, winter sleigh hats) |
| `crowd` | NPCs in view (`OW_BB_MAX` billboards, island chibis) | 2 s / 8 s | crowd bed (low babble) + how full the town arrangement is |
| `activity` | input and movement | idle after 20 s, rest after 90 s | thinning to I1, then a planned rest (5.12) |
| `wall_dist` | cells from the Wall (Ch.1-2) | continuous | WALL / Ominous bleed (4.4) |
| `zone` | `story_region_name()` regions | at the banner | the region variant and the ambient bed |
| `sanity`, `corruption` | resident needs | 2 s / 10 s | detune, WOBBLE, darker reharm pool |
| `monster` | the town monster meter | daily | rarer major colours; the "starer" silence gag gets more frequent |
| `possessed` | the count of possessed residents | daily | the island's `infect` |
| `comfort`, `spook`, `fort` | Dream House (story spec) | 4 s / 30 s | HOME's state and stems (4.17) |
| `pet_stage`, `pet_line`, `pet_need`, `pet_sleep` | `pet.h` | event | BEEP's form, the pet cue variant |
| `tension` | fishing line tension | instant / 0.5 s | reel cue intensity, dissonance near snapping |
| `heat` | cooking heat, welding heat | instant / 1 s | tempo +0..15 %, pitch climb, alarm |
| `timer` | minigame time left (fraction) | continuous | final 10 s: tempo +15 %, hats in 16ths, key +1 |
| `hype` | concert applause meter | 1 s / 6 s | crowd claps, choir doubling, encore key change |
| `trust` | Chapter 4: Hellvile citizens helped / redeemed | per quest | how *redeemed* the Riff is (G→G♯, B♭→B probabilities) |
| `love` | relationship level on the resident screen | event | LOVE quotes, maj7 colours |
| `altitude` | Cloud Nine Spire floor | per floor | key +1 semitone and tempo +2 per floor |
| *comic events* | the comic-timing engine (5.9) | event stream | stings, mutes, scratches, elastic tempo |

**Intensity scale** used throughout the cue sheet:

| Level | Name | Typical stems |
|---|---|---|
| I0 | Bed | the ambient bed only (silence is allowed) |
| I1 | Sketch | bass + pad, or the music box alone |
| I2 | Groove | + light drums + chord arps |
| I3 | Theme | + lead melody (the normal "song") |
| I4 | Lifted | + counter-melody, echo, busier drums, fills |
| I5 | Peak | + choir / crowd / metal doubles / crash on downbeats |

---

## 4. Cue sheet

**Legend.** *Tempo · key · dial* gives the home values (arrows = journeys).
*I* = intensity range. *Drivers* are from section 3. Cue IDs (`MC_*`) are the
proposed enum. Where an existing `SONG_*` is named, it becomes that cue's
A-phrase. "Quote" refers to the quoting system (5.11). Transition
quantisation: `Q:bar` (next bar), `Q:beat`, `Q:phrase` (next 4/8-bar
boundary), `Q:now`.

### 4.1 Boot, title, menus, system

| Cue | When | Mood and form | Tempo · key · dial | I | Drivers → effect | Motifs, layers, transitions |
|---|---|---|---|---|---|---|
| `MC_BOOT` | SCENE_BOOT splash | the trailer s01 gag: a music-box shimmer over C augmented resolves to C... and **freezes mid-arpeggio** when the devil tail flicks (a SCR_STOP scratch, 0.2 s), then a single "beep." | free · C · S0 | I1 | - | BEEP egg form; ends in silence so the title can hit |
| `MC_TITLE` | SCENE_TITLE | the full Hook ("Doomogachi Life!", today's `SONG_TITLE`) with graph sections A A' B (Hook in G) C (the Riff "teased" as a cute bass line) | 140 · C · S1 | I3-I4 | idle 60 s → **"corruption creeps"**: the next phrase plays in E minor with hell drums (S3) and a vape hiss bed; any button → SCR_NEEDLE scratch + "nothing to see here!" sparkle, back to A in C | the title-theme unlock picks the variant (the wishlist's title themes): Cute / Spooky / Metal / Lullaby renders of the same graph |
| `MC_NEW_ISLAND` | SCENE_NEW_ISLAND | a preview per hovered option: **Empty** = the music box alone; **Classic** = full toy pop; **Random** = "dice" reharmonisation, a new chord pool every 2 bars | 132 · C · S0-S1 | I1-I3 | hover → stem set, Q:bar | 2-note BEEP on confirm |
| `MC_CREATOR` | SCENE_CREATOR | "Make a Friend!" (today's `SONG_CREATOR`); **one stem per creator tab**: face = bells, hair = arps, body = bass, outfit = disco hats, voice = the babble choir sings the preview voice | 132 · D → per species · S1→S2 | I2-I4 | how hellish the creation is (horns, demon species, eyes ≥ 3) → the dial S1→S2, minor reharm; "Yes, three eyes." = a STG_GASP | each tab change Q:beat; randomise = SCR_SCRIBBLE; finish = MAYOR fanfare |
| `MC_MENU` | options, save/load, unlocks gallery, quest log, the pause panel | the current area cue **ducked −9 dB, low-passed, mono**: the world keeps playing behind glass | - | - | open → LPF/duck in 4 ticks | save = HOME full phrase; load = HOME doorbell |
| `MC_MONSTER_TOGGLE` | Options: "I AM A MONSTER" confirmation | 1st confirm: the menu cue stops (SCR_STOP); 2nd: a blast-beat stab + evil choir chord + "beep." (trailer s11) | - | I5 stab | - | STG_EVIL_LAUGH |
| `MC_NEWS` | SCENE_NEWS ticker | a newsreel jingle (the Hook head on brass pulse, 4 bars) then a light bed under the ticker | 120 · F · S1 | I2 | gloomy headline (possession, death) → the same jingle in F minor | each item = a tiny ticker blip |
| `STG_UNLOCK` | any "NEW UNLOCK!" toast | BEEP adult + sparkle, 1 bar, `Q:beat`, music ducks | - | - | - | pet line colours it |

### 4.2 Free-play island hub (`SCENE_ISLAND`): time × weather × town state

**One cue, `MC_ISLAND`, built from `SONG_HAHALAND_DAY` (F major, 124) and
`SONG_HAHALAND_NIGHT` (G major, 80) as phrase pools.** Time of day is a
continuous journey, not a switch. The day ↔ night handover happens on a
phrase boundary within ±15 game minutes of the band edge.

| Band | Hours | Tempo | Harmony colour | Stems at I3 | Ambient bed | Notes |
|---|---|---|---|---|---|---|
| Dawn | 05-08 | 108 → 116 | sus2 / add9, pedal tones | music box + triangle + light shaker | birds (pulse chirps tuned to the key's pentatonic), a rooster blip at 06:00 | starts at I1 and blooms; the first Hook of the day is always the music box |
| Morning | 08-11 | 120 | plain major, bright | toy pop full | birds + distant playground | Hook A / A' |
| Noon | 11-14 | 124-128 | major + secondary dominants | full + counter-melody | crowd murmur | the most energetic; B and C sections allowed |
| Afternoon | 14-17 | 122 | maj7 creeping in | full, swing 2 | bees, sprinklers | a lazier groove |
| Golden hour | 17-19 | 116 | maj7 / 9, IV-iv borrowing | lead → flute-triangle, echo on | cicadas | key down a 4th (F → C) for the B section: "warm" |
| Dusk | 19-21 | 104 | vi-centred, ♭VI–♭VII–I | sparse; PWM pads | crickets begin | prepares the night pool |
| Night | 21-01 | 80 | G major lullaby (`SONG_HAHALAND_NIGHT`) | music box, harp arps, soft hats | crickets, fireflies (high pulse twinkles) | HOME and BEEP quotes allowed |
| Late night | 01-05 | 72 | pedal-heavy, open fifths | I1 only, long rests | wind, an owl (triangle "hoo hoo") | 5.12 rests are longer here |

**Weather modifiers** (apply on a phrase boundary):

| Weather | Drums | Harmony | Lead and arrangement | Bed | PS1 extra |
|---|---|---|---|---|---|
| CLEAR | per band | per band | per band | per band | Room reverb |
| RAIN | **brushes**: long-decay noise, low volume, no claps | borrows iv and ♭VI (gentle sadness) | lead → 50 % pulse, slower vibrato; the Hook's tail resolves *down* | rain (noise LFSR with a slow random-walk volume) + drip blips | reverb → Studio Medium |
| SNOW | half-time; **sleigh hats** (short 7-bit noise, high clock) | sus4 → major, pedal | 12.5 % "celesta" lead an octave up, sustained pads | a soft wind hush | Hall reverb |
| PETALS | + shaker | add9 | **sparkle**: a glockenspiel double an octave up on every melody note at I3+ | breeze | Studio Small |

**Town-state modifiers:**

- `possessed` / `infect` > 0 adds a Vex droop on phrase ends (5.11). At ≥ 3
  possessed, a ♭5 drone fades in under night bands.
- `monster` (the town monster meter): the reharm pool loses its brightest
  options, and once per in-game day the **starer gag** may fire: the music
  hard-cuts for 2 beats of total silence on a downbeat, then resumes as if
  nothing happened.
- **After the ending** (`MAPID_PARK_UNITED` unlocked): `MC_ISLAND` gains the
  "Park United" phrase pool. The Riff (redeemed) answers the Hook in
  call-and-response, and the Hellvile folk swing becomes the B section.
- Location hover on the hub map previews the building's motif (2 bars, Q:bar).

### 4.3 Free-play buildings and screens

| Cue | When | Mood and form | Tempo · key · dial | I | Drivers → effect | Motifs, layers, transitions |
|---|---|---|---|---|---|---|
| `MC_APT_HILL` | SCENE_APARTMENTS, Hillvile Towers | lobby bossa: the Hook as elevator music, maj7 | 100 · F · S1/L | I2 | floor scroll → 4.3a | ding = HOME 2 notes |
| `MC_APT_HELL` | SCENE_APARTMENTS, Hellvile Pits | lo-fi Riff, swing 6, vinyl crackle | 92 · E minor · S2 | I2 | - | crackle at 20 % |
| *4.3a windows* | each lit window | **the resident's favourite genre** (`like_music`, 16 genres) as a 2-bar micro-loop in the lobby's key and tempo. The centred window is loud; neighbours are muffled, like walking down a corridor | lobby tempo | I1-I2 | activity: sleeping → lullaby in any genre; partying → disco; problem bubble → minor; **possessed → the window's loop plays in retrograde (DECK reverse) and a fifth lower** | 16 genre styles shared with the Studio's genre table |
| `MC_ROSTER` | SCENE_ROSTER / PICKER | "file cabinet": a light pizzicato pulse groove | 116 · C · S1 | I1-I2 | district tab → the Hook or Riff flavour | page flip = 16th blip |
| `MC_RESIDENT` | SCENE_RESIDENT | the district theme at I1-I2, coloured by the resident's mood | district | I1-I3 | happy → bright major; sad → GOODBYE fragments; angry → Riff chugs on bass; in love → LOVE; possessed → a ♭5 pedal + reversed babble bed; `corruption` → reharm darkness | each care action = a 1-beat sting (feed / gift / clean / play / heal / comfort), Q:beat |
| `MC_ROMANCE` | confession, proposal, date (resident screen events) | **LOVE**: build-up from I1 to I3 on a held suspension (sus4 → 3) | 96 · F · S1 | I1→I4 | accepted → LOVE full + sparkle (I4); rejected → SCR_STOP + STG_SAD_TROMBONE | cross-border couple → LOVE over the Riff (2.10) |
| `MC_WEDDING` | marriage | an *original* processional (no Mendelssohn/Wagner): LOVE in 3/4 on hymn organ, ending on Fuzz's amen | 72 · F · H | I3-I5 | - | the couple's TRUE NAME motifs as the bridge (Hellvile partners) |
| `MC_BABY` | a baby is born | BEEP-style lullaby in augmentation: **angel babies** = harp maj7; **hellspawn** = the same with a tiny tritone tag | 84 · C · S0 | I2 | - | STG_TADA on arrival |
| `MC_BREAKUP` / `MC_ARGUMENT` | breakups, arguments you resolve | breakup: GOODBYE at I1; argument: a comic "bickering" call-and-response between two pulses, panned L/R, resolving to a unison when fixed | 100 · A minor | I1-I3 | resolved → unison + STG_FRIEND | - |
| `MC_SHOP` | SCENE_SHOP | "Shop Hop" (`SONG_SHOP`) | 116 · D · S1 | I2-I3 | browse speed → hat density | buy = a cash-register blip (pulse sweep up); BLACK MARKET (Hellvile) = `MC_SHOP` in E minor, S2, swing 6 |
| `MC_GRAVEYARD` | SCENE_GRAVEYARD | GOODBYE (`SONG_SAD`) variations over a night bed | 66 · A minor · S0/H | I1-I2 | day/night; a fresh grave → organ | **Lay to Rest** = GOODBYE on organ + Fuzz amen; **ghost rite** = an ethereal choir, reversed cymbal swells (pre-reversed samples) and the citizen's TRUE NAME motif made translucent (12.5 % pulse, echo) |
| `MC_PLACE_*` | free-play location menus (Cafe, Hospital, Town Hall, Beach, Fun Fair, Arena, Wall/Gate) | each place's own cue at I1-I2 behind the menu (4.4 has the full list) | - | - | - | - |

### 4.4 Story overworld (`SCENE_STORY`), by chapter, map and region

Base cue per map, then region variants (`story_region_name()`), then the
chapter's arc. Positional emitters (a door leaking music, the Wall's dread)
blend by distance.

**MAPID_HAHALAND, Chapter 1 "Welcome to HaHaland" (pure sugar):**

| Region / place | Cue | Tempo · key · dial | I | Notes |
|---|---|---|---|---|
| Town Plaza | `MC_HAHALAND` (= `MC_ISLAND` pools + the story's chapter colour) | 124 · F · S1 | I2-I4 | `crowd` → fullness; time of day and weather as in 4.2 |
| Hillvile Lane, Town Hall Hill | `MC_HAHALAND`, Mayor flavour | 124 · F · S1 | I2-I3 | the MAYOR fanfare quoted near the Town Hall |
| Hillvile Towers | `MC_HAHALAND` + bossa B section | 120 · F · S1/L | I2 | - |
| St. Cuddles Square | `MC_HAHALAND` with hymn harmonies (plagal cadences) | 116 · F · S1/H | I2 | FUZZ head quotes near the church door |
| The Fun Fair | calliope: the Hook on a 12.5 % "organ" with oom-pah bass | 132 · C · S1 | I3 | Fun Fair Big Top door leaks `MC_FAIR` |
| Hellvile Park, Hillvile Side | pastoral: flute-triangle lead, park birds | 112 · C · S0-S1 | I2 | **`wall_dist`**: within 12 cells the WALL motif bleeds in under the music (low pulse, tritone pedal), with the purple-fog bed |
| The East Bank | river: a 3/4 lilt, ripples | 108 · G · S1 | I2 | - |
| HaHa Beach / The Pier | surf-chip: twangy 25 % pulse with echo, triangle bass | 128 · A · S1 | I2-I3 | waves bed |
| Your Cottage (exterior) | HOME doorbell | - | - | interior → `MC_HOME_*` (4.17) |
| Nana's (Brimstone Bites) | NANA polka | 150 · F · S1 | I2 | - |
| Rivet's Workshop | `MC_WORKSHOP` (4.12) leaks from the door | - | - | - |

**Chapter 2 "Something in the Vapor"** keeps `MC_HAHALAND` but turns up
`infect` with every beat:

| Beat | Change |
|---|---|
| C2_VAPOR | **Cloud Nine Vapor** opens next to the Wall. A positional **Vex lounge** (`MC_VAPE_LOUNGE` = `SONG_VAPE_LOUNGE`, 88 · A minor · L) leaks out of the door. `infect` 32. |
| C2_WEIRD | `infect` 96: the Hook's tail droops chromatically on 1 phrase in 3, and the harmony pulse detunes by about 10 cents |
| C2_SCREAM / FUZZ / EXORCISE | `MC_OMINOUS` (today's override) with WALL quotes; the scream = SCR_STOP + silence (1 bar) + WALL sting |
| C2_RIVET | Rivet's motif enters the counter-melody near the workshop: the first major-third "redeemed" riff |
| C2_CELLAR | `infect` 192: the ♭5 pedal; at the cellar door the Hell Breach 1 intro sting |

**MAPID_HAHALAND_INVADED, Chapter 3 "Hell Comes to HaHaland":**

| Region | Cue | Tempo · key · dial | I | Notes |
|---|---|---|---|---|
| everywhere (base) | `MC_INVASION` (`SONG_INVASION` = A phrase) | 168 · E minor · S4 | I2 (no enemies) → I5 | `danger` and `enemies` → stems; **a nest encounter happens on the same map, so the cue never changes**: an encounter = a fill + crash into I4 on the next bar; clearing a nest = a 2-bar breakdown (I2) + STG_TADA in metal |
| Town Plaza | full war march | 168 | I3-I5 | ember bed |
| HaHa Beach | **surf-metal** (the surf-chip lead over S4 drums) | 168 · E minor | I3-I5 | - |
| St. Cuddles Square, inside the church | **safe zone: "Hope"**: the Hook on an acoustic-ish triangle and music box, the Riff **muffled outside** (LP + volume, S4 still running) | 84 · C · S0 | I1-I2 | entering/leaving = a crossfade on the next bar; no combat here |
| a rescued citizen | **"ray of cute"**: the Hook head on a toy pulse over the metal for 2 bars | - | - | quote, Q:bar |
| possessed citizen nearby | a ♭5 drone + the citizen's TRUE NAME on a wobble pulse | - | - | - |
| the Wall / the torn gate | `MC_OMINOUS` in metal (WALL motif on power chords) | 92 half-time | I4 | the Spire is visible: Vapula's motif on brass, far away |

**MAPID_HELLVILE, Chapter 4 "Hellvile" (and free-play Hellvile Visits):**

| Region / place | Cue | Tempo · key · dial | I | Notes |
|---|---|---|---|---|
| base | `MC_HELLVILE` (`SONG_HELLVILE`, retuned to E minor) | 108 · E minor · S2 | I2-I4 | dusk-locked (Hellvile is clamped to dusk); `trust` turns the Riff redeemed bit by bit |
| Gate Plaza | the Riff + the gate's drone | 104 | I2-I3 | - |
| The Pits | lo-fi swing (`MC_APT_HELL` pool) | 92 | I2 | - |
| Forge Row / Anvil's Forge | RIVET in minor (her family) → after "Rivet goes home", the major/minor duet | 110 · G mixolydian ↔ G minor | I3 | anvil percussion |
| Imp Market Quarter / Imp Market | **busker**: the Riff on an "accordion" (stacked 25 % + 12.5 % pulses with tremolo), oom-pah | 116 · E minor · S2 | I3 | crowd bed (low babble in Hellvile voices) |
| Chapel Hill / Mother Nox's Chapel | NOX hymn (F minor, kind) | 72 · F minor · H | I2 | at the festival → the Fuzz/Nox canon |
| Vape Factory Ruins | Vapula's motif, broken: a detuned music box, half-tempo | 70 · E minor · S0 | I1 | an "it's over" ruin: rubble bed |
| The Sealed Gate | WALL drone, Riff augmented in the bass (the gate "breathes") | 60 · E | I1-I2 | after the ending: "covered in flowers" = HOME in E major, birds |
| Hellvile Arena | metal-lite hype; `MC_ARENA_LOBBY` | 150 · E minor · S3 | I3 | - |
| Crypt Row | spooky-cute harpsichord pulse | 96 · D minor · S2 | I2 | crypt "for rent" jokes get a STG_RIMSHOT |
| The Lake of Fire / Bone Pier | `MC_FISH` night variants (4.14) | - | - | - |
| prisoners (C4_PRISONERS) | **"Prisoners' Lament"**: the Riff slow and minor in 3/4, one pulse + a sad wave organ | 72 · E minor · S0-S2 | I1-I2 | each prisoner helped: `trust` +; the lament's next pass plays one more redeemed note |
| C4_MAYOR | the MAYOR fanfare in E minor → E major as he crosses over | - | - | Q:bar, a 2-bar showpiece |

**MAPID_PARK_UNITED** (C4_FESTIVAL, C4_STAGE, free play): `MC_FESTIVAL` =
**COMMINGLING** (2.9) as an open phrase graph. Regions: **The Unity Stage**
(the full anthem, crowd, I4-I5); **Hillvile Meadow** (the Hook-led mix,
Riff under); **The River** (a quiet duet: one Hook pulse and one Riff pulse
across the water, panned L/R); **Hellvile Garden** (Riff-led mix, Hook
under); **Parade Road** (march feel, 4.19); **Festival Stalls** (each stall
leaks its own mini-cue: Nana's polka, the Imp busker, Vex's water stand in A
major).

### 4.5 Cutscenes and emotional beats

The cutscene player gets cue + sting control per panel (the comic-timing
engine, 5.9, owns the timing). Story cutscenes:

| Cutscene | Today | Direction |
|---|---|---|
| `CUT_CHAPTER1` | `SONG_TITLE` | a chapter card: the Hook on the music box (2 bars) → full toy pop on the title drop; ends on alt C (the bright button) |
| `CUT_CHAPTER2` | `SONG_VAPE_LOUNGE` | the Hook card again... but its last note slides down (Vex's droop), and the lounge takes over: "Something in the Vapor" |
| `CUT_CHAPTER3` | `SONG_INVASION` | a metal slam card: the Riff in octaves, a crash, SCR_SPINBACK into silence, then the title text |
| `CUT_CHAPTER4` | `SONG_HELLVILE` | the Riff on a lone wobble pulse, swung... and the very last note is the **redeemed** B: a hint |
| `CUT_ARRIVAL` | `SONG_HAHALAND_DAY` | **wonder**: the dawn band of `MC_ISLAND`, the Hook blooming from the music box to I3 as the town is revealed; MAYOR fanfare on his entrance |
| `CUT_HOME` | `SONG_PET` | HOME doorbell → the pet answers with BEEP (its current stage): the house and the pet in dialogue |
| `CUT_FOG` | `SONG_OMINOUS` | WALL motif; the Vex motif on a single breathy pulse, far away and panned left (the side the vape shop is on) |
| `CUT_ERUPTION` | `OMINOUS` → `INVASION` on panel 2 | **trailer s08 exactly**: the park party (the Hook full tilt for 2 beats) → **SCR_NEEDLE** → 1 beat of silence → "beep." → the sky cracks: a low E power-chord BWAAM, a chromatic tremolo + snare roll building to the ROAR → the Riff at full metal on the downbeat |
| `CUT_VAPULA_DOWN` | `SONG_VICTORY` | Vapula's motif **deflates** (SCR_STOP over 2 beats, pitch down two octaves), silence, a tiny "pocket-sized" squeak, then VICTORY rendered in **E major**, the key of the ending |
| `CUT_WALL_FALLS` | `SONG_FESTIVAL` | **trailer s13**: the Hook, shy, on the music box ("...and maybe, just maybe...") over a C pad; B trembles under the rumble; the wall **crumbles = key change to E major**: COMMINGLING enters full; a dead cut on the joke beat (if the scene has one), then the anthem returns |
| `CUT_FREEPLAY` | `SONG_CREDITS` | the united anthem in its S1 form: "life goes on" |

**Story beat emotional map** (the overworld cue and the sting at each beat):

| Beat | Emotion | Music move |
|---|---|---|
| C1_ARRIVE, MAYOR | wonder, welcome | dawn bloom; MAYOR fanfare |
| C1_KEY, HOME | belonging | HOME doorbell; the save phrase on first save |
| C1_HATCH | joy | the egg pet cue (4.11): beep → crack → the BEEP baby form, STG_TADA |
| C1_PIP, BUNNY | worry → play | PIP motif in minor ("Mr. Flops is gone!") → the search uses **hot/cold** (5.11): the harmony detunes when far, locks in when near |
| C1_BUNNY_RETURN | relief | PIP motif in major + squeak + "aww" sus4 → 3 |
| C1_FRIENDS, CUPCAKE | social glow | STG_FRIEND per friend (a rising major 3rd, then a 5th, then the octave on the third friend) |
| C1_WALL | dread | `MC_OMINOUS`; the purple fog bed |
| C2_VAPOR → CELLAR | creeping wrongness | `infect` ramp (4.4) |
| C2_SCREAM | shock | SCR_STOP + silence + WALL sting |
| C2_FUZZ, EXORCISE | faith, comedy | FUZZ hymn; the exorcism cue (4.10) |
| C2_RIVET | camaraderie | RIVET funk; the choice of cute vs brutal starter = a toy-pulse sting vs a power-chord sting |
| C2_CELLAR | dare | Hell Breach 1 intro (4.7) |
| C3_ERUPTION | catastrophe | CUT_ERUPTION |
| C3_NESTS | defiance | `MC_INVASION`; each nest cleared = a breakdown + metal ta-da |
| C3_SPIRE | ascent | Hell Breach 2 (4.7) and Vapula (4.8) |
| C4_GATE | crossing over | the Wall theme resolves: WALL `e f e b-` → `e f+ e b` (the redeemed spelling) as the gate opens |
| C4_PRISONERS, HELP | empathy | Prisoners' Lament with rising `trust` |
| C4_MAYOR | reconciliation | MAYOR fanfare, minor → major |
| C4_FESTIVAL, STAGE | catharsis | COMMINGLING; the stage show = a concert-style performance (4.16) of the "Park United!" template |
| DONE | gratitude | credits (4.19) |

**Episodes and deeper chapters (story spec):** each episode gets an
**episode card** (2 bars: the chapter's motif plus an episode-specific
reharmonisation), a **"previously on..." recap** (SCR_REWIND into a
low-passed medley of the last episode's cues), and a **cliffhanger out**
(SCR_STOP or a freeze-frame + one WALL/Vapula note, then silence).

### 4.6 Side missions (80+) and quest jingles

Side missions keep the overworld cue and add **family layers and stings**,
so 80 missions do not need 80 songs.

| Family | Layer while active | Stings |
|---|---|---|
| Fetch / deliver | none (the world cue) | `STG_QUEST_NEW` (the Hook head, 4 notes) on accept, `STG_QUEST_STEP` (2 notes) on progress, `STG_QUEST_DONE` (a small MAYOR fanfare) on hand-in |
| Find / lost pet | BEEP-variant counter-melody + **hot/cold** tuning: a harmony voice detunes up to 40 cents when far and locks in near the target | a found sting: the pet's adult BEEP |
| Photo | a light shutter-click hat on the beat while aiming | "cheese!": a sparkle + click on Q:beat |
| Matchmaking | LOVE quotes near either target | success: a LOVE duet (two pulses in thirds) |
| Exorcism request | FUZZ quotes near the church and the client | the church cue on hand-off |
| Cross-border reconciliation | the Hook and Riff in **call-and-response**, the Riff's tritone still there | resolved → the redeemed Riff answers in unison (`trust` +) |
| Mini-boss hunt | a combat-lite stem set on the world cue (`danger` driven) + the boss motif teaser | defeat sting (4.8) |
| Timed delivery / race | a ticking clock-hat layer; tempo follows the time left (`timer`) | late → STG_SAD_TROMBONE |
| Chase / escort | the world cue double-time (drums only), Q:bar | caught → SCR_STOP |
| Post-game / secret sparkles | secret sparkle = a music-box arpeggio up (key-aware), 1 bar | all secrets → a hidden jukebox cue |

### 4.7 Hell Breach levels 1-7

Each level has a **cute half** and a **hell half**. The level's hell blend
drives the style dial continuously, and the existing `hell_at` threshold
becomes the point where the dial reaches S4 (the phrase graph switches to the
hell pool on the next bar). **First person vs third person:** in third
person the stereo widens (twin leads hard L/R, the power-chord stack wider)
and the player's own footsteps are sidechained into the hats. **Gore CUTE vs
BRUTAL:** CUTE renders S4 as **toy metal** (the same riff on a toy pulse,
music box and claps, squeaky percussion, trailer s10); BRUTAL renders the
real thing. "Gore: BOTH?!" alternates every 4 bars.

| Level | Cute half | Hell half | Tempo · key | I | Level-specific drivers and gimmicks | Mini-boss (4.8) |
|---|---|---|---|---|---|---|
| **HB1 The Vape Cellar** | Vex's lounge **muffled through the ceiling** (the shop is upstairs) + crates and dust bed | `SONG_HELL_BREACH` | 88 lounge → 150 · A minor → E minor | I1-I4 | descending stairs: the lounge low-passes further each floor; the tutorial beats get Q:beat stings (jump, lift, secret = a music-box "ta-da") | Cellar Wraith |
| **HB2 Cloud Nine Spire** | windy, floaty half-time with echo (the cloud bridge) | `SONG_BOSS` pool before the top | 120 → 150 (+2/floor) · E minor **+1 semitone per floor** (`altitude`) | I2-I5 | **lifts play muzak**: Vapula's motif as bossa elevator music (a comic relief bar between fights); lava sections add a low wave rumble; the key climbs with the tower | Lord Vapula (3 phases) |
| **HB3 Sugar Rush Sewers** | strawberry-soda calypso: bubbling blips (pulse 1 hardware sweeps up), steel-drum-ish 50 % pulse | the same melody, **wet**: the wave channel morphs to "gross" cycles, the pitch wobbles, gurgle bed | 132 → 150 · C → E minor | I2-I5 | `hell` from 200: soda turns fleshy; the calypso bass becomes a sludgy saw | Gumdrop Gutterking |
| **HB4 Plushie Factory** | **conveyor groove**: machinery as percussion (stamping presses on 1 and 3, belt rattle hats), plush squeaks as accents, a factory-whistle 3-note call | industrial chip metal, the 3+3+2 accent pattern | 140 → 160 · D → E minor | I2-I5 | conveyor speed → hat density; "Now hiring: souls" card = the whistle motif | Foreman Fluffnuts |
| **HB5 Graveyard Shift** | goth-cute: harpsichord pulse (12.5 %, short), organ wave, night bed | `SONG_HELLVILE` pool → S3-S4 | 96 → 132 · D minor; **down a semitone each loop** (it sinks) | I1-I4 | the weeping in the garden: a distant broken music box (the Widow's motif) grows louder by distance | The Weeping Widow |
| **HB6 Bone Cathedral** | the church cue gone wrong: organ hymn, bell tower | `SONG_HELL_BREACH` pool + organ metal | 72 → 150 · F minor | I2-I5 | the level goes UP: each floor adds a register to the organ; bells (PS1: PMON FM bells; GBA: 12.5 % + noise transient) on landmarks | Bishop Bonebottom |
| **HB7 Hellvile Underbelly** | **Imp Market busker** (the Riff on accordion-pulse), pipes and pistons (pitched steam hisses as a percussion section) | `SONG_INVASION` pool, then the gate's drone | 116 → 168 · E minor | I2-I5 | pistons = a 7/8 mechanical ostinato under the 4/4 riff (the gate engine "limps") | Gatekeeper Grumbleguts |

### 4.8 Bosses and phases

**Common boss grammar.** The intro banner and taunt get a **2-bar intro
sting** with the boss motif (Q:bar). The fight loop is a phrase graph keyed
by `boss_phase`. Phase changes are **SCR_SPINBACK → downbeat of the new
phase** (Q:bar). At boss HP < 10 % the drums drop to a half-time breakdown
and the choir enters ("the last stand"). The defeat line = SCR_STOP, 1 beat
of silence, then the defeat sting. Boss Rush chains bosses with SCR_REWIND
transitions.

| Boss | Motif | Phase plan | Tempo · key | Defeat |
|---|---|---|---|---|
| **LORD VAPULA**, Duke of the Ninth Cloud (HB2, Boss Rush) | VAPULA (2.5) | **P1 "The Duke Descends"** (100-66 %): half-time doom, the motif in augmentation on saw + evil choir, vape-hiss bed (PS1: NON hiss). **P2 "Cloud Burst"** (66-33 %): full thrash, the motif in diminution, twin leads in harmonic-minor thirds, blast-beat bursts on his special attacks. **P3 "Ninth Cloud"** (33-0 %): **up a semitone**, the motif **inverted** (rising smoke), double-time, and the **Hook enters as a counter-melody**: HaHaland fights back. < 10 %: breakdown + choir | 92 feel → 184 → 192 · E minor → F minor | SCR_STOP (2 beats) = deflation, silence, "LORD VAPULA IS DEFEATED!" → VICTORY in **E major** (CUT_VAPULA_DOWN) |
| **THE CELLAR WRAITH** (HB1), "Brimstone Blast Flavoured" | VEX in metal | P1 the lounge motif on power chords with slides; when hurt, it "coughs" (SCR_STUTTER) | 150 · A minor | "dissolves into a fruity cloud": the lounge chord with a long echo tail + a sparkle |
| **GUMDROP GUTTERKING** (HB3) | **sticky slides**: notes slide and *stick* a quarter-tone below pitch, then snap up | P1 calypso-metal; P2 (his soda splash) everything detuned, wobble | 150 · E minor | "melts into a sad syrup puddle": SCR_STOP very slow (1 bar), drip blips |
| **FOREMAN FLUFFNUTS** (HB4) | factory whistle 3-note + a time-clock "punch" | "BREAK TIME IS OVER, NEW HIRE!" → **tempo jump** +16 bpm at each phase; P2 adds the conveyor 7/8 | 160 → 176 · E minor | "clocks out. Forever.": the whistle in a descending glissando + a punch-card "ka-chunk" |
| **THE WEEPING WIDOW** (HB5), "lost her plushie in 1887" | **a broken music box**: a waltz in D minor with missing notes | the fight is **tragic, not evil**: 3/4 waltz, music box + ghost choir, S2 at most; her attacks swell the strings-pulse | 96 (3/4) · D minor | "stops crying, waves, and fades away": her waltz **resolves to D major**, the missing notes return, GOODBYE quote. A touching moment, no victory fanfare |
| **BISHOP BONEBOTTOM** (HB6), "The Organ Grinder" | the **anti-hymn** (2.6) | P1 the organ hymn in minor; P2 organ metal; P3 the bell tower (bells on every downbeat) | 150 · F minor | "the organ plays one last, very sour note": a scripted detuned cluster + SCR_STOP, then Fuzz's amen in major (the church wins) |
| **GATEKEEPER GRUMBLEGUTS** (HB7) | the gate engine: a mechanical 7/8 ostinato | P1 the engine + the Riff; P2 the engine overheats (tempo creeps up, pitch rises); P3 the engine stutters (SCR_STUTTER every 2 bars) | 168 · E minor | "the gate engine sputters out. Hellvile breathes.": the engine SCR_STOPs and **the Hellvile swing Riff plays in major for the first time** (the Underbelly seeds the ending) |

### 4.9 Invasion encounters, the Arena and arena modes

| Cue | When | Mood and form | Tempo · key · dial | I | Drivers → effect | Notes |
|---|---|---|---|---|---|---|
| `MC_INVASION` encounters | story nests, invaded map | 4.4 | 168 · E minor · S4 | I2-I5 | `danger`, `enemies`, `hp`, `combo` | no cue change on entering combat: a fill + crash on the next bar |
| `MC_COMBAT` generic | SCENE_COMBAT from other contexts | the map's music (`d->music`) promoted to a combat stem set | per map | I2-I5 | same | - |
| `MC_LEVEL_SELECT` | SCENE_LEVEL_SELECT | Hell Breach theme at I1-I2 + **card previews**: hovering a level plays its cute-half motif for 2 bars (Q:bar) | 150 · E minor · S3 | I1-I2 | rank of the hovered level → a fanfare colour | - |
| `MC_RESULTS` | after a level | rank jingles: **S** full metal fanfare (VICTORY on power chords); **A** VICTORY; **B** VICTORY on toy pulse; **C** a single "ta-da" with a sad end note | - | - | new record → + STG_UNLOCK | - |
| `MC_DEATH` | player death / game over | SCR_STOP (the "power down"), 1 beat of silence, then GOODBYE in 4 bars (`SONG_SAD` today) | 66 · A minor | I1 | - | Retry → SCR_REWIND into the level cue at its last checkpoint section |
| `MC_ARENA` | SCENE_ARENA (free-play waves) | the Invasion pool; **wave start** = a crash + 1-bar fill; every 5 waves the key goes up a semitone | 160 · E minor (+1/5 waves) · S4 | I3-I5 | wave number → tempo +2 per wave (cap +24) | wave clear = 2-bar breakdown |
| `MC_HORDE_SHOP` | Horde mode shop breaks | **"Shop Hop" in metal-lounge**: S2-S3, a countdown click-hat for the break's remaining time | 116 · D → E minor | I2 | `timer` | last 3 s: SCR_REWIND into the next wave |
| `MC_BOSS_RUSH` | AM_BOSS_RUSH | all seven boss cues in order, joined by SCR_REWIND transitions; the HP bar between bosses = a 2-bar "breather" on the music box | per boss | I4-I5 | - | the final boss (Vapula) gets all three phases |
| `MC_TIME_TRIAL` | Time Trial | the level's cue + a **pace click** on the hats; when the run falls behind par, the tempo nudges up (+1 bpm per 2 s behind) | per level | I3-I5 | `timer` vs par | gold = S fanfare |
| `MC_CUTE_RUN` / `MC_BRUTAL_RUN` | Cute-Only / Brutal-Only runs | the dial is **locked**: S1 toy metal / S4 full metal | per level | - | - | - |
| `MC_MAYHEM` | Monster Mode Mayhem (3 minutes) | **chaos metal**: 200 bpm blast beats, an evil choir with a tritone on top, every motif **inverted**, and a random scratch pattern every 8 bars (seeded) | 200 · E♭ minor | I5 | final 30 s: key up every 8 bars | ends with SCR_SPINBACK + "beep." |

### 4.10 Exorcism (`SCENE_EXORCISM`)

| Phase | Cue | Mood and form | Tempo · key · dial | I | Drivers → effect |
|---|---|---|---|---|---|
| Intro (the church) | `MC_CHURCH` (`SONG_CHURCH`) | FUZZ hymn, organ | 72 · F · H | I2 | host `sanity` → how many organ voices |
| **1 Prayer rhythm** | `MC_RITUAL` (`SONG_EXORCISM`) | **the music is the game**: the prompts are the chart. **Fuzz's organ calls** (the FUZZ head), **the demon answers** with the Riff. Every hit adds a choir "ah" stab; every miss lets the demon laugh (STG_EVIL_LAUGH, short) and detunes the organ by 5 cents | 120 · E minor ↔ F · H vs S3 | I2-I5 | streak → the demon's answers lose layers (S4 → S1); `demon strength` → the Riff's volume and the tritone |
| **2 Holy water** | `MC_RITUAL` B pool | an ostinato with 16th hats that **accelerate as the demon darts** | 120-140 | I3-I4 | demon speed → hat density; hit = sizzle + "ow. my everything." babble pitched down; miss = splash + STG_BOING |
| **3 True name** | `MC_TRUE_NAME` | suspense: a drone, a clock tick at heart rate, and **each clue reveals one more note of the possessor's TRUE NAME motif** (2.10) | free · E minor · S0 | I1 | clues seen → notes revealed; wrong pick → SCR_STOP + a buzzer; right pick → the full motif resolves **into Fuzz's amen** |
| Success | `MC_EXORCISED` | the biggest plagal amen in the game: choir + organ + VICTORY in hymn form | 72 · F | I5 | - |
| Failure ×1, ×2 | stings | trailer s11: STG_SAD_TROMBONE (spray), the hug-fail "wah", the bucket "boing" | - | - | fail count → the gag escalates |
| Soul lost | `MC_SOUL_LOST` | GOODBYE (`SONG_SAD`) | 66 · A minor | I1-I2 | - |
| Lay to Rest | `MC_GRAVEYARD` funeral form | GOODBYE on organ + Fuzz amen | 60 · A minor → C | I2 | friends who grieve → each adds a quiet pulse voice |
| Monster Mode | `MC_MONSTER` stab | trailer s11: a snare roll as the cursor sneaks down, then blast beat, tremolo chugs, an evil choir with a tritone and a dive-bomb squeal | 200 | I5 | - |

### 4.11 The Doomogachi pet (`SCENE_PET`)

**Deliberately the most limited music in the game, right next to the most
limit-pushing.** The LCD screen gets **piezo mode**: one or two 50 % pulse
voices, no envelopes, no drums. On the GBA that is PSG pulse 2 only; on the
PS1 one voice with a baked "tiny speaker" cycle (a rattly 50 % with a buzzy
3rd harmonic). That limit is the joke.

| State | Cue | Notes |
|---|---|---|
| Idle | `MC_PET` (`SONG_PET`, "Doomogachi Beeps", 150 · C) | the BEEP of the current stage and line as the A-phrase head |
| Egg | one beep every 2 bars | wobble on "about to hatch" |
| Hatch / evolve | a fanfare: piezo arpeggio up, then **for exactly 1 bar the LCD "breaks" into the full band** (S1 for cute and angel lines, S4 for the demon line), then snaps back to beeps. This is trailer s04's "four frames of hell" made musical | - |
| Hungry / dirty / sick / sad | the BEEP head in minor (hungry); a "plop" + tuba-pulse (poop); a detuned wobble (sick, the WOBBLE scratch pattern at 3 %); slower tempo (sad) | the worst need (`pet_need`) wins |
| Sleeping (lights off) | 60 bpm, one beep per bar, the music-box lullaby form of BEEP | - |
| Care mistake | an error buzz (pulse 12.5 % low) | - |
| Play minigame | 2-note call-and-response: the pet beeps left or right and you answer | right = an octave beep; wrong = a buzz |
| Discipline | a stern low beep + a "hmph" sweep down | - |
| The demon line, adult | the LCD tune plays normally... and on every 4th loop its last bar is the 4-frame hell glitch | the trailer's "...it's fine." |

### 4.12 Workshop and welding

| Cue | When | Mood and form | Tempo · key · dial | I | Drivers → effect |
|---|---|---|---|---|---|
| `MC_WORKSHOP` (`SONG_WORKSHOP`) | SCENE_WORKSHOP | RIVET funk: slap bass, anvil, gallop hats | 110 · G mixolydian · S1-S3 | I2-I3 | browsing BRUTAL parts → S3, CUTE parts → S1; naming = a typewriter tick on 16ths |
| `MC_WELD` | SCENE_WELD_GAME | the workshop groove as the weld beat: **bead in the zone = the groove holds** | 110 → 140 | I3-I4 | `heat` → tempo +0..15 % and pitch climb; out of the zone → SCR_STUTTER; overheat → a siren (pulse 1 sweep up/down) + the trailer gag: "DAYS SINCE LAST FIRE: 0" = STG_RIMSHOT |
| Weld result | sting | a quality grade jingle (BURNT-style sad trombone → PERFECT anvil fanfare) | - | - | the generated weapon name gets a Q:beat ta-da |

### 4.13 Fun Fair minigames (`SCENE_MG_*`)

Shared flow (mg.c): title card → READY? GO! → PLAY → TIME UP / OUCH →
RESULTS. **Tiers:** Sweet = S1; Spicy = S2-S3 and +10 % tempo; **Hellfire =
S4** in the hellish night version (the existing `mg.hell` → `SONG_HELL_BREACH`
becomes each game's own melody in metal). **Final 10 s** (`timer`): tempo
+15 %, hats in 16ths, key +1. **TIME UP** = SCR_STOP. **Results**: the score
count-up is a rising arpeggio ticker locked to the count; NEW BEST = VICTORY.

| Game | Host | Cue | Tempo · key | Notes |
|---|---|---|---|---|
| Cupcake Catch | Nana Pudding | NANA polka | 150 · F | each catch = a pitched blip climbing the scale (a combo = a melody) |
| Whack | Whackamole | a comedic galop | 168 · C | each whack lands on the off-beat accent if on time |
| Dodge | **Lord Vapula** | "Vapula's Dodge-o-Rama": his motif as circus lounge (S1/L) | 140 · A minor | near miss = a scratch chirp |
| Gallery | Rivet | RIVET funk, shooting-gallery bells | 126 · G | targets pop on 8ths |
| Derby | Eyeballer (bookie) | an original race galop | 176 · D | the leader's racer gets the melody; photo finish = SCR_STOP + freeze |
| Memory | Madame Moth | a mysterious celesta waltz | 108 · E minor (3/4) | each revealed card plays its note (cards are tuned: a matched pair = a chord) |
| Wheel | (prize wheel) | an arpeggio **locked to the wheel's ticks**: the music accelerates with the spin and decelerates with it | wheel speed | landing = STG_TADA or STG_SAD_TROMBONE |

### 4.14 Fishing and cooking

| Cue | When | Mood and form | Tempo · key · dial | I | Drivers → effect |
|---|---|---|---|---|---|
| `MC_FISH` idle | SCENE_FISHING, waiting | **lazy pluck-pulse "fishing blues"**: day (ukulele-ish 50 % pluck), twilight (maj7 lounge), night (fireflies + lullaby; Lake of Fire night = a Hellvile tinge in E minor) | 92 · G / D / E minor · S1/L | I1-I2 | time of day → variant; lapping-water bed |
| Cast | sting | a whoosh + the reel click as a 16th-note roll | - | - | cast power → whoosh length |
| Bite | sting | the Hook head as a **pizzicato "nibble"** (two plucks, then the 3rd), Q:beat | - | - | rarity → a brighter colour |
| `MC_FISH_REEL` | reeling | drums enter; **`tension` → intensity**: the line near snapping = a dissonant trill + high noise; the fish running = a bass slide down | 128-150 | I3-I5 | `tension` |
| Catch | jingle | per rarity tier: common a tiny 2-note ta-da; rare MAYOR fanfare; legendary full fanfare + choir | - | - | - |
| Junk | jingle | comedic: **Lord Vapula's Lost Sock** = his motif on a kazoo-pulse with a sad slide | - | - | each junk item has a 1-bar gag |
| `MC_COOK` | SCENE_COOKING, Granny Hopscotch | kitchen ragtime | 132 · F · S1 | I2-I3 | - |
| CHOP | rhythm game | **the knife is percussion** (PCM chops, GBA soft PCM or noise 7-bit); the music's tempo = the chop tempo | 120-160 | I3 | on-beat chops = accents in the groove |
| STIR | motion game | the arpeggio loop speed follows the stir speed (a DECK scrub at 0.8-1.2×) | - | I2 | stir speed |
| BAKE / FRY / SIMMER | timer game | a ticking clock and rising tension; FRY = a sizzle bed whose pitch follows `heat` | - | I2-I4 | `heat` |
| Grade | jingles | BURNT: sad trombone + smoke-alarm beeps; OK: plain; GREAT: jingle; PERFECT: fanfare + Nana-style approving babble | - | - | - |
| Counter | serving customers | diner swing | 116 · B♭ | I2-I3 | customers served → crowd bed; a happy customer = STG_FRIEND |

### 4.15 The Grand Library (`SCENE_LIBRARY` and wings)

| Wing | Cue | Notes |
|---|---|---|
| Lobby | ghost librarians: **"shhh"**: the music stays at I1 (music box + pad); a page-turn bed; any loud sting gets a babble "shh!" | 84 · C · S0 |
| Bestiary | a spooky catalogue: the Riff on a harpsichord pulse (chamber) | 100 · E minor · S2 |
| Almanac | cosy, HOME-flavoured | 92 · F |
| Weapons | Rivet funk, light | 110 · G |
| Items | Shop Hop, light | 116 · D |
| Trophies | brassy, MAYOR quotes on hover | 120 · C |
| **Jukebox** | plays any unlocked cue. **Version toggle**: Cute / Spooky / Rock / Metal / Lullaby / Piezo / **Generative** (the live phrase graph, endless) / **Classic** (today's fixed songs). Plus a **"Scratch it!" button** (a random scratch pattern on the playing cue) and a **Studio export** ("open this cue's A-phrase in the Studio") | - |
| Gallery | elegant museum: S0 + Fuzz harmonies | 80 · F |

### 4.16 Music Studio and Concert Hall

| Cue | When | Notes |
|---|---|---|
| `MC_STUDIO` | SCENE_STUDIO editor | silence (or the song being edited); the background music is stopped as today |
| Studio **SCRATCH lane** | the drum track's scratch layer (6.6) | players place scratch patterns; preview = the pattern played on the current song |
| Studio PERFORM | the choir show | the song + crowd bed; applause = noise swells; encore = key +1 |
| `MC_CONCERT_LOBBY` | SCENE_CONCERT before the show | the stage's tuning-up noodle (each instrument enters with a random lick in the song's key) |
| Stages (`STAGE_*`) | the act | **Sugar Bowl** (S1 cute pop, PS1 Studio reverb); **Disco Inferno** (disco S2 with a metal tinge, Hall reverb, mirror-ball arps); **Graveyard** (goth, Space Echo); **Hellfire** (S4, Room, pyro = crash + noise swell); **Dreamland** (dream pop: WOBBLE pattern, Echo reverb) |
| `hype` | crowd meter | crowd claps (PCM on both consoles) at 128+, choir doubling at 192+, the **encore key change** +1 semitone |
| **DJ Booth** (new act, `CMODE_DJ`, append-only) | a citizen DJ (Lo-fi Beats, Synthwave, Hardcore Punk and Industrial fans have the best "skill") plays the generated or studio song **and scratches it live**, with patterns chosen by personality | a **DJ Battle** = two DJs alternate 4-bar scratch solos; **"Take the decks"** = the player scratches live (6.6 controls), scored like the exorcism prayer rhythm on the scratch timing |
| Choir (up to 25 singers) | both consoles | GBA: the lead babble + up to 3 section voices (soft PCM vowel chops) + PSG harmony; PS1: up to 8 choir voices (SATB × 2, detuned) from vowel ADPCM loops + the babble voice. The 25 singers still all bop and sing visually |

### 4.17 The Dream House (story spec: GBA isometric, PS1 2.5D rooms)

**One cue, `MC_HOME`, built on HOME (2.8).** Each **room is a stem**: the
room you stand in is in front; adjacent rooms play muffled (low-passed and
quieter), so walking through the house *is* mixing the song.

| Room | Its stem | Bed |
|---|---|---|
| Living room | **the radio**: the last jukebox pick, low-passed and in mono, with crackle (TAPE-style vinyl) | clock tick |
| Kitchen | utensil percussion: clinks and pot taps (PCM; GBA noise 7-bit "clank") | kettle |
| Bedroom | the music box (HOME lead) | soft breathing when someone sleeps |
| Bathroom | everything **reverberant** (PS1 Hall; GBA soft echo at 60 % feedback) + bubble blips | drips |
| Nursery / pet corner | BEEP descant | the pet's sleep beeps |
| Workshop / garage | the RIVET anvil on beat 4 | - |
| Attic | sparse, detuned, dusty | creaks |
| Basement | an ominous drone: **the Riff muffled through the floor**, because Hellvile is underneath | a low rumble |
| Garden | flute-triangle + birds | birds (key-aware) |

**House variants** (driven by `comfort`, `spook`, `fort`; a crossfade at a
phrase boundary, with a SCR_REWIND transition when it flips suddenly):

| Variant | Trigger | Arrangement | Tempo · key · dial | I |
|---|---|---|---|---|
| **Cosy** | `comfort` high, `spook` low | the HOME waltz; harp arps join as `comfort` rises; guests → a crowd-babble bed and a LOVE/FRIEND quote | 84 · C (3/4) · S0-S1 | I1-I4 (by `comfort`) |
| **Haunted** | `spook` ≥ 128 (ghost residents, nightmares, possessions in the house) | HOME in C minor without its resolution; the **WOBBLE** warped-record pattern; ghost "ooh"; furniture SFX get reversed-cymbal pre-swells | 76 · C minor · S0-S2 | I1-I3 (by `spook`) |
| **Barricaded** | `fort` > 0 (an invasion at the door, the story's siege beats) | HOME as a 4/4 tom march, hammering SFX quantised to the beat; when demons reach the door, the Riff arrives in the bass | 120 → 168 · C → E minor · S2-S4 | I2-I5 (`danger`) |
| **Party** | a hosted party (story / free play) | disco S1: octave bass, open hats, LOVE and Hook quotes | 120 · C · S1 | I3-I4 |
| **Sleep** | the player rests (`OPT_REST`) | HOME lullaby → silence → the morning doorbell (a bird + HOME 3 notes) | - | I1 |

**Comic beats in the house** (via the comic engine): the fridge opening =
a sting chord; a ghost behind the player = SCR_STOP + silence + "boo"; the
toilet = STG_RIMSHOT. The house is where the comedy timing gets most
domestic use.

### 4.18 Dreams and nightmares (`SCENE_DREAM`)

| State (`DS_*`) | Cue | Notes |
|---|---|---|
| PEEK | HOME lullaby in the sleeper's favourite **genre** (re-voiced to S0) | the moon twinkle = a high pulse ping, key-aware |
| ENTER | **a bubble rise**: a harp glissando up (all soft voices glide), then the dream cue on the downbeat | Q:bar |
| DREAM (3 vignettes) | **dream-wobble** of the sleeper's genre: tempo 0.9×, the WOBBLE pattern, reversed cymbals (pre-reversed samples), heavy echo / Echo reverb | **as the twist types out and the picture turns from cute to creepy, the dial rises by 2 within the vignette (cute → creepy in 180 ticks)**, then the next vignette resets |
| NIGHTMARE | the last vignette goes red: the WOBBLE deepens into a "melting record" (velocity sags to 0.8 and drifts), heartbeats, Riff/Vapula quotes, and the **sleeper's fear sting** (below) | wake them (a hug) → SCR_STOP, the bedroom returns warm; let them sleep → the drone continues into AFTER, colder |
| AFTER | HOME at I1 in major or minor by the outcome | - |

**Fear stings** (one per `doomo_fear_names` entry, used in nightmares and
in comic beats): Spiders = a chromatic pizzicato crawl; The Dark = every
voice drops out except a heartbeat; Thunder = a noise boom + a low wave
blip; Clowns = a calliope in minor; Ghosts = "ooh"; Heights = a pitch drop
(slide whistle down); Deep Water = muffled bubbles; **Vacuum Cleaners** = a
noise riser that keeps rising; Bees = 7-bit buzz; Needles = high
staccato; Mirrors = the motif played in retrograde; Being Alone = one
voice, far left; Public Speaking = a microphone feedback squeal (pulse 12.5 %
at 3 kHz) + crowd murmur; Bath Time = the bathroom echo; Dentists = a drill
(saw at 2 kHz with vibrato); Popping Balloons = random pops; **Silence = the
music cuts completely, then one heartbeat**; Holy Water = a church organ
chord; **Sunlight = full-sugar HaHaland Day** (terrifying to demons); Church
Bells = bells; Exorcisms = Fuzz's amen, very loud; **Hugs** = a warm major
chord swelling closer; **Cute Things** = the music box Hook at maximum sugar
(a Hellvile sleeper's nightmare is HaHaland); Tax Season = adding-machine
clicks on 16ths.

### 4.19 Credits parade (`SCENE_CREDITS`)

The parade marches pairs (Hillvile id *n* beside Hellvile id 500 + *n*)
every `RANK_TICKS` = 11 ticks (0.367 s), for roughly 5,500+ ticks (about 3
minutes or more). **The march locks to the music**: the cue runs at **163.6
bpm in a half-time anthem feel (81.8)**, so **each pair steps on a quarter
note**. (Alternatively story can change `RANK_TICKS` to match a chosen
tempo.) Hold A fast-forwards: DECK velocity 1.5 (pitched up, like a record
at the wrong speed), back to 1.0 on release with a short scratch.

| Section | Who passes | Music |
|---|---|---|
| Intro (cards 1-3) | "DOOMOGACHI LIFE in HELLvile Park!", the team, "Starring @P and @E" | the Hook on the music box → BEEP adult descant |
| A | ranks 1-100 | COMMINGLING A: the Hook over the redeemed Riff |
| Cameo cards | Mayor Giggs | MAYOR fanfare in E |
| | Nana Pudding | NANA polka bar |
| | Father Fuzz | the FUZZ head + amen |
| | Rivet | RIVET anvil (major and minor family thirds) |
| | Pip & Mr. Flops | PIP + squeak |
| | Mother Nox | NOX mirror hymn |
| | Vex ("now selling plain water") | VEX in **A major**, tritone straightened |
| | Lord Vapula ("now pocket-sized") | his motif, **deflated**: staccato tuba-pulse two octaves up |
| A' | ranks 101-250 | call-and-response: "Hillvile calls, Hellvile answers" |
| B (bridge) | ranks 251-350; **the citizens who passed away march translucent** | HOME in 4/4 augmentation, softer, GOODBYE harmonies, echo (in loving memory) |
| C | ranks 351-450 | the Fuzz/Nox hymn canon over a pedal E, with the crowd |
| Final chorus | ranks 451-500 + "And you" | **+1 semitone to F major**, every layer stacked, fireworks = noise swells and crashes |
| End card | "Thank you for playing!" | **Fuzz's amen** (IV - I) and a single "beep." (the existing `audio_music_once(SONG_VICTORY)` becomes the amen tag) |

Each pair's names in the ticker may carry their TRUE NAME motifs (Hellvile
side) as tiny pings, rate-limited to one per bar.

### 4.20 Stingers and jingles

All stingers duck the music by 6-9 dB for their length, are `Q:beat` by
default, and are **rendered in the current style dial** where marked "dial".

| ID | Sound | Use |
|---|---|---|
| `STG_QUEST_NEW` / `STEP` / `DONE` | the Hook head 4 notes / 2 notes / a small MAYOR fanfare (dial) | quests |
| `STG_CHAPTER_CLEAR` | a full MAYOR fanfare + crash (dial) | chapter ends |
| `STG_UNLOCK` | BEEP adult + sparkle | unlock toasts |
| `STG_FRIEND` | a rising major 3rd "boop-boop" | friendship made |
| `STG_LOVE` / `STG_HEARTBREAK` | LOVE bar / LOVE falling + SCR_STOP | romance |
| `STG_TADA` | I-V-I bright (dial) | small successes |
| `STG_FAIL` | a descending minor 3rd buzz | small failures |
| `STG_RIMSHOT` | "ba-dum-tss": tom, tom, crash (noise) | jokes |
| `STG_SAD_TROMBONE` | 4 chromatic falling notes on a 50 % pulse with vibrato and slides ("wah-wah-wah-waaah") | fails, exorcism gags |
| `STG_SLIDE_UP` / `STG_SLIDE_DOWN` | slide whistle (**GBA pulse 1 hardware sweep**; PS1 pitch-register glide) | pratfalls, heights |
| `STG_BOING` | a fast up-down sweep | bounces |
| `STG_HONK`, `STG_KAZOO` | a short saw blat / a saw with vibrato slides (trailer `SFX_KAZOO`) | clowning |
| `STG_BEEP` | the single "beep." | the running gag |
| `STG_ITS_FINE` | a cheery C major chord with **a wrong F♯ in the bass** that decays last | "it's fine." running gag |
| `STG_GASP` | a sus4 stab, held | shocks |
| `STG_REVEAL` | a dim7 stab + a reversed cymbal swell (pre-reversed sample) | reveals |
| `STG_DUN_DUN_DUNN` | the WALL tritone stabs on power chords | dramatic irony |
| `STG_HALLELUJAH` | Fuzz's amen on choir | holy moments, exorcism success |
| `STG_EVIL_LAUGH` | the Riff head in octaves + a babble laugh pitched down | villains |
| `STG_SPLAT` | a noise burst + low thump | splats |
| `STG_IRIS` | the music shrinks: LP + mono + volume to one music-box note | iris-out transitions |
| `STG_DEFEAT` / `VICTORY` | the existing `SONG_SAD` / `SONG_VICTORY`, rendered in the current dial | combat ends |
| `STG_SCRATCH_*` | any scratch pattern as a sting (section 6) | everything comedic |

---

## 5. Non-repetitive yet still looping: the toolkit

The core idea is to **loop the *grammar*, not the audio**. A cue is a
graph of short composed phrases plus rules for walking it, varying it and
layering it, all seeded and deterministic. It can play forever with no
seam and no audible repeat. A playback position still exists (the graph
node and bar), so the music can be quantised, resumed and scratched.

### 5.1 Phrase graphs

- **Node (phrase):** 2, 4 or 8 bars. It holds MML per *role* (LEAD, COUNTER,
  HARM, BASS, KIT, PERC2, PAD, METAL, CHOIR, FX), plus its **entry chord**,
  **exit chord** (as functions: I, IV, V, vi, ♭VII...), intensity bounds and
  flags (`ANCHOR` = carries the theme head, `TRANSIT`, `BREAKDOWN`, `CADENCE`,
  `INTRO`, `REENTRY`).
- **Edge:** a weight (0..255) and conditions (intensity range, dial range,
  time of day, "after ≥ N passes", chapter).
- **Walk rules**, applied in order:
  1. **No immediate repeat:** A→A is illegal (A→A' is fine; variants are
     separate nodes that share data).
  2. **Recency penalty:** weight × ¼ if the node was picked in the last 3
     picks, × 0 in the last 2.
  3. **Bag guarantee:** a node not heard for 2 × (node count) picks gets
     weight × 4, so every phrase comes around (as in a Tetris 7-bag).
  4. **Anchor rule:** an `ANCHOR` node at least every 3 picks, so the theme
     stays recognisable, and never more than 2 anchors in a row.
  5. **Cadence legality:** exit chord → entry chord must be in the cue's
     voice-leading table. If the game state leaves no legal edge, the
     arranger generates a **1-bar turnaround** (V7, ii-V, ♭VI-♭VII-I, or a
     chromatic slide into the target chord).
  6. **Arc shaping:** picks are grouped in "chapters" of 4-6 phrases that
     follow an intensity contour (build → peak → release) inside the
     driver-given range, so even a static game state gets musical arcs.
- **Example, `MC_HAHALAND`** (nodes from today's `HAHALAND_DAY` and `TITLE`
  material):

```
INTRO(2) ──1.0──> A(8)
A   (anchor, Hook)            ──.55─> A'   ──.30─> B   ──.15─> BRK
A'  (anchor, Hook alt bars)   ──.50─> B    ──.30─> C   ──.20─> BR
B   (counter-melody, IV-centred) ─.40─> A  ──.35─> C   ──.25─> BR
C   (relative minor, LOVE quote slot) ─.50─> A' ─.30─> BR ─.20─> TAG
BR  (bridge, key up a 4th)    ──.60─> A'   ──.40─> B
BRK (breakdown I1: bass + music box, 4 bars) ─.70─> A  ─.30─> C
TAG (2-bar button, half cadence) ──1.0──> A or REST (5.12)
```

  Even before per-pass variation (5.2), this graph's
  10-minute walks differ in more than 90 % of their 8-bar windows.

### 5.2 Per-pass melodic variation

Every statement of a node is varied by a **variation level** V0..V3 per role
(V0 = literal). The first statement of a cue in a session is V0, and the
level rises with the pass count. **Head protection:** the first 3-4 notes of
any motif are never altered except by transposition or mode mapping. Ops,
seeded by `(cue seed, pass, node, role)`:

| Op | What | Level |
|---|---|---|
| Grace / mordent / turn | a 32nd from the upper scale neighbour; a 3-note turn on long notes | V1 |
| Slide-in / fall-off | a 2-tick slide from −2 semitones; a fall at phrase ends | V1 |
| Vibrato depth and delay | per pass ±25 % | V1 |
| Anticipation / lay-back | push a strong-beat note an 8th early (pop syncopation) or 16th late | V2 |
| Dotted swap, split, merge | 8 8 → 8. 16; a long note → repeated 8ths; repeated notes → a tie | V2 |
| Octave displacement | single notes ±1 octave (never a leap > 10th) | V2 |
| Tail inversion / retrograde / sequence / truncation | mirror the tail in the scale; reverse it; repeat it a step higher or lower; end early with a rest | V3 |
| Call-and-response swap | the lead and the counter trade phrase halves; or the bass takes the head in augmentation while the lead sustains | V2 |
| Timbre swap | the lead moves PSG pulse 1 → 2 → soft voice (new duty, new pan) | V1 |
| Passing-tone fill | passing tones inside long notes at I4+, removed at I2 | V1-V3 |
| Echo-by-repeat toggle | the line repeated on a second channel 1/8 or dotted-1/8 later at −6 dB | V1 |

**Guard rails:** strong beats keep chord tones; added notes snap to the
cue's scale (the core already has `song_scale_snap`-style helpers); ranges
per instrument; no more than 2 ops on one phrase at V3.

### 5.3 Reharmonisation pools

Per melody note on a strong beat, the arranger may swap the default chord
for another from the pool of the current **dial** (seeded, V2+, never on an
`ANCHOR` node's first bar). Reference: C major (cute) and E minor (spooky /
metal).

**Major (Hillvile, S0-S1).** The colour by area: Hillvile adds add9 and
maj7 extensions; the United park adds mixolydian ♭VII ("festival rock") and
plagal IV-I.

| Melody degree | Default | Cute pool | Borrowed / spooky pool |
|---|---|---|---|
| 1 (C) | C | Am7, Fmaj7, Dm7 | A♭maj7, Fm |
| 2 (D) | G | Dm7, Gsus4, B♭(add9) | Bdim7 |
| 3 (E) | C | Am, Em7, Cmaj7, A7 (V/ii) | C+, Am(maj7) |
| 4 (F) | F | Dm, B♭, G7 | Fm, D♭maj7 (Neapolitan) |
| 5 (G) | C | Em, G, Am7 | E♭ (♭III), Cm |
| 6 (A) | F | Am, Dm, D7 (V/V) | B♭maj7 (lydian ♭VII) |
| 7 (B) | G | Em, E7 (V/vi), Cmaj7 | Bø7 |

**Minor (Hellvile S2; Hell S3-S4).** Hellvile's colour is m(maj7), dim7 and
the Neapolitan F. Hell uses power chords on ♭II (F5) and ♭V (B♭5).

| Melody degree | Default | Spooky-cute pool | Metal pool (power chords: melody = root or 5th) | Redeemed (`trust`) |
|---|---|---|---|---|
| 1 (E) | Em | C, Am, Fmaj7 (N) | E5, A5, C5 | E |
| 2 (F♯) | B7 | D, Bm, F♯ø | B5, F♯5 | B, D |
| ♭3 (G) | Em | C, G, E♭+ | G5, C5 | (G♯) E |
| 4 (A) | Am | F, D, B7(♭9) | A5, D5 | A |
| 5 (B) | B7 | Em, G | B5, E5 | E, G♯m |
| ♭6 (C) | C | Am(maj7), F (N) | C5, F5 | (C♯) A |
| 7 (D♯) | B7 | D♯dim7, G+ | B5 | B |
| ♭5 (B♭, the tritone) | - | E7♭5 | B♭5, E5 (pedal) | → B: E or B |

**Voicing and arp variation:** arp patterns (up, down, up-down, broken
thirds, Alberti, seeded random-in-chord, the classic 3-note 50/60 Hz chip
chord); inversion picked by minimal voice movement; chord rhythm (block,
off-beat stab, Charleston, arps). **Bass variation:** roots, root-fifth,
octave bounce, walking (chromatic approach on beat 4), pedal, Charleston,
and the metal gallop (8th-16th-16th).

### 5.4 Generative drums and humanised velocity

- **Groove DNA:** each style has a 16-step probability table per drum,
  masked by intensity. Examples (probability in tenths, 0-9 or X = always):

```
S1 toy pop (I3)          1 . . . 2 . . . 3 . . . 4 . . .
 kick                    X . . . . . 3 . 9 . . 2 . . 3 .
 clap/snare              . . . . X . . . . . . . X . . 2
 hat (closed)            7 . 9 . 7 . 9 . 7 . 9 . 7 . 9 .
 shaker (16ths)          . 3 . 3 . 3 . 3 . 3 . 3 . 3 . 3     (I4+)

S4 chip metal (I4)
 kick (PCM double)       X 9 X 9 X 9 X 9 X 9 X 9 X 9 X 9
 snare                   . . . . X . . . . . . . X . . 1
 crash/china             X . . . . . . . . . . . . . . .     (bar 1 of phrases)
 hat / ride              8 . 8 . 8 . 8 . 8 . 8 . 8 . 8 .
```

- **Fill grammar** at phrase ends (the last 1/2, 1 or 2 beats; bigger fills
  lead into higher intensity): snare roll (16ths → 32nds, crescendo); tom
  cascade (PSG noise at descending clock dividers = pitched toms; PCM toms);
  kick-snare flams; toy clap stutter; metal blast; **stop-time** (all drums
  drop for 2 beats, then a crash: comedy gold); a reversed-cymbal swell
  (a pre-reversed sample: PCM in the GBA ROM, ADPCM in PS1 SPU RAM).
- **Crash rule:** a crash on a section downbeat only after a fill or an
  intensity rise.
- **Velocity humanisation:** an accent table (downbeat 15, beats 13, 8ths 10,
  16ths 8 on the PSG's 0..15 scale) + seeded jitter ±1 (±3 on the 0..63
  scale of soft voices and SPU voices) + ghost snares (level 4-5) whose
  probability rises with intensity.
- **Micro-timing:** soft voices and the PS1 can offset events by ±1
  microtick (4.2 ms): a laid-back snare (+1) in Hellvile swing, pushed hats
  (−1) in punk and metal. PSG drums sit on the 240 Hz microtick grid too.
- **Round-robin timbre:** the PSG noise clock divider varies per hat hit
  (brightness), and 7-bit mode is used for flams. The PS1 kits alternate
  2-3 samples per drum with ±20 cents of pitch (no machine-gun effect).
- **Separate RNG:** drums use their own stream, so a melody change never
  shifts the groove (5.13).

### 5.5 Vertical stems and intensity

- Each node's roles are **stems** with an intensity threshold and a
  **render priority** (used by the platform tiers, section 7.4).
- **Hysteresis:** up and down thresholds are half a level apart; a 4-bar
  minimum dwell; danger spikes may jump up on the next beat behind a fill.
  Stems drop at a phrase end unless intensity fell by 2+ levels.
- **Clean fades on both consoles:** **PSG stems fade by density** (the
  pattern thins note by note, because volume changes retrigger PSG square
  notes and click). **Soft voices and SPU voices fade by volume** (per-block
  ramps on the GBA, voice volume or hardware volume sweeps on the PS1).
- **Entry:** stems enter on a bar downbeat or with their own pickup.
- **Example, `MC_INVASION`:**

| I | Stems |
|---|---|
| I2 | drone (wave: E power-chord cycle, long), ride hats, ember bed |
| I3 | + Riff (bass + power-chord HARM), snare backbeat |
| I4 | + lead (sync saw), PCM double kick, the Vapula quote slot |
| I5 | + twin harmony (thirds, L/R), evil choir (PS1 4 voices, GBA 1 soft PCM), crash every bar |

### 5.6 Key and tempo journeys

- **Lift:** +1 or +2 semitones after N passes (the festival, credits,
  concert encores), at most twice, then a pivot back down.
- **Pivot through the shared cell:** any Hillvile cue can modulate to E minor
  (or a Hellvile cue to C) through a phrase that starts with E-E-G, which is
  valid in both keys. This is how the world flips: the invasion, crossing
  the gate, the redemption.
- **Ladders:** the Spire (+1 per floor), the Graveyard Shift (−1 per loop,
  sinking), arena waves (+1 every 5 waves), minigame final 10 s (+1).
- **Time-of-day tempo curve** (4.2) and mediant shifts for "something's off"
  (C → E♭ in Ch.2 infection B sections).
- **How tempo changes:** at a phrase boundary with a 1-bar ramp; by
  **double-time or half-time feel** (the drums change, the tempo does not;
  preferred in combat to keep everything locked); or by elastic
  micro-stretches for comic timing (5.9).
- **Limits:** ±8 % tempo per phrase outside scripted moments; keys stay in
  the cue's allowed set, so the motifs keep their identity.

### 5.7 Cute ↔ metal morphing

- The dial value (0..255 internally, 5 bands) selects the **instrument set
  per role, groove DNA, harmony pool, articulation** (gate lengths, palm
  mute), **bass pattern, mode mapping** of the melody, and **tempo feel**.
- **Mode mapping** by scale degree: major → natural minor (3→♭3, 6→♭6,
  7→♭7), or keep the 7 for harmonic minor (Hellvile S2). "Hellify" touches
  only the head's peak note (5 → ♭5).
- **Heroes go major metal, villains go minor metal.** The Hook in S4 is E
  *major* (the trailer's anthem). The Riff and Vapula in S4 are minor.
- **Morph order at intermediate values:** parts swap one at a time, in
  priority order **drums → bass → harmony → lead**. The melody is the last
  thing to turn, so the listener follows the change.
- **Fast morph** (an invasion trigger): SCR_NEEDLE or SCR_REWIND + a silence
  beat + every part at S4 on the downbeat.
- **Gore CUTE** = the S4 groove with toy instruments ("toy metal"). Options
  **AU14** (Invasion music: Metal / Cute remix / Keep location theme) caps
  the dial: Cute remix = S4 grooves on S1 instruments; Keep = no dial
  change. **AU15** (Dynamic combat layers) Off = a fixed I3.

### 5.8 Quantised transitions and stingers

- **Grid:** NOW, 16th, BEAT, BAR, PHRASE (4/8 bars), NODE_END. A request
  carries (target cue/node, grid, type, max_wait). If the grid point is
  further away than `max_wait` (gameplay needs a response), it falls back to
  a finer grid.
- **Types:**

| Type | Behaviour | Typical use |
|---|---|---|
| CUT | stop at the grid point; the new cue starts on its downbeat, no fade | smash cuts |
| CROSS | a 1-2 bar stem crossfade (volume on soft/SPU voices, density on PSG) | area changes |
| FILL → | a generated fill in the old cue, then the new downbeat with a crash | entering combat |
| PICKUP | the new cue's anacrusis starts on beat 3 or 4 of the old bar | motif entrances |
| PIVOT | a shared-cell modulation (5.6) | world flips |
| SCRATCH | REWIND / SPINBACK / STOP / NEEDLE / STUTTER (section 6) | comedy, phases, retries |
| SILENCE | a planned 1-4 beat rest before the new downbeat | dead air before a reveal |
| STING | a sting with ducking, then resume (or skip ahead to keep the phrase alignment) | stingers |

- **Tempo bridging:** the new cue starts on the old cue's next downbeat;
  scratch transitions hide tempo differences (the velocity curve blends
  them).
- **Resume, not restart:** returning to a cue (from a menu, shop or combat)
  resumes its graph position or enters a `REENTRY` node. Today
  `audio_music()` restarts a song only if it differs; keep that, and add the
  resume.

### 5.9 Comic-timing integration

The comic-timing engine (story spec, borrowed from the trailer's
`timeline.c` / `music.c`) sends **events** with a **land frame**. The music
engine answers with a quantised action and reports where it will land.

| Comic event | Music response | Grid | Pre-roll |
|---|---|---|---|
| `CE_SETUP` | keep playing; thin by one intensity level to give the dialogue room | BAR | - |
| `CE_PAUSE` | mute window: drop to the bed for N beats ("dead air") | BEAT | - |
| `CE_DEADPAN` | **absolute silence**, beds included ("Yes, the actual PlayStation.") | NOW | - |
| `CE_PUNCH` | a sting on the **reaction** frame, picked by joke flavour (below) | NOW or BEAT (elastic) | the sting's hit offset |
| `CE_SMASH_CUT` | the old cue CUTs on the frame; the pre-armed new cue's downbeat is that frame | NOW | arm ≥ 2 ticks ahead |
| `CE_FREEZE` | SCR_STOP or SCR_NEEDLE timed so the *stop* lands on the freeze frame; reverb freeze (PS1) or echo freeze (GBA) | pre-rolled | pattern length |
| `CE_RECORD_SCRATCH` | "wait, WHAT?": SCR_NEEDLE → silence until `CE_RESUME` | pre-rolled | ¼ beat |
| `CE_SLOWMO` | DECK velocity → 0.5 over ½ beat, hold, back to 1 on resume | BEAT | - |
| `CE_REWIND` | SCR_REWIND into the flashback cue (low-passed, mono, crackle) | BAR | 1 beat |
| `CE_STUTTER` | SCR_STUTTER (the last 1/16, 4 times): spit takes, glitches | NOW | - |
| `CE_IRIS_OUT` | STG_IRIS | NOW | - |
| `CE_IMPACT` | kick + crash + duck (screen shake, flash) | NOW | - |
| `CE_RUNNING_GAG` | "beep." / "it's fine." / the starer's silence / Vapula's crown "tink" / Mr. Flops' squeak | BEAT | - |
| `CE_RESUME` | re-enter at the next downbeat with a 2-beat pickup, or continue where paused | BAR | - |

**Three ways to land a joke on the beat** (the comic engine picks per
event; it queries `mus_ticks_to(grid)` first):

1. **Hold the picture:** wait for the beat if it is ≤ `hold_max` away
   (default 6 ticks = 0.2 s).
2. **Bend the music:** stretch or compress the rest of the current beat by
   up to ±12 % so the next beat lands on the punch frame (inaudible at that
   size; the microtick clock makes it exact).
3. **Ignore the grid:** sting on the exact frame; the groove resumes with a
   pickup on the next downbeat.

**Pre-roll and latency.** Stings and scratches whose hit is not at their
start (SCR_STOP's stop is at its end) are started at `land − hit_offset −
platform latency` (GBA DirectSound: 1 frame; PS1 TAPE stream: 2 frames;
DECK: 0). The comic engine schedules ≥ 3 ticks ahead.

**Ported from the trailer's score** (`trailer/music.c`): write the music
continuously, then **carve mute windows** for comic beats and **duck
windows** under the SFX the joke hangs on; a note starting inside a mute
window is dropped, a note running into one is cut at its start; tape stops
and held breaths ignore windows. API: `mus_mute(from, to)`,
`mus_duck(from, to, gain)`.

**Joke flavour → sting:** pratfall → STG_SLIDE_DOWN / STG_BOING; awkward →
STG_ITS_FINE or `CE_DEADPAN`; reveal → STG_REVEAL / STG_DUN_DUN_DUNN;
failure → STG_SAD_TROMBONE; win → STG_TADA; gross → STG_SPLAT; holy →
STG_HALLELUJAH; evil → STG_EVIL_LAUGH; cute overload → sparkle + an "aww"
sus4→3; "(We judge a little.)" → one disapproving low pulse "tsk" (Fuzz).

**House rules for funny:** silence *before* the punchline (1-2 beats); the
sting goes on the *reaction*, not the line; never two stings within a bar;
outside DJ contexts at most one scratch per 8 bars (more stops being funny);
"beep." always follows at least a beat of silence.

### 5.10 Ambient beds

- Beds are **generative emitters**, not loops: each emitter has an interval
  range (seeded), a pitch set **taken from the current key's pentatonic**
  (so birds never clash with the music), volume and pan ranges, and gating by
  time, weather and zone.
- **Emitters:** birds (GBA pulse-1 chirps with hardware sweeps; PS1 PMON
  chirp pairs), crickets (12.5 % pulse trills gated at 50 Hz), wind (noise
  with a slow random-walk volume), rain + drips, river ripples, lava bloops
  (low wave blips + noise), vape hiss, crowd murmur (a low babble generator
  with random resident voices, unintelligible), clock tick, machinery,
  church hum, **embers (they reuse the vinyl crackle generator)**, fireworks
  (noise bursts + whistle sweeps), page turns.
- **Voices:** on the GBA beds use the PSG noise channel while the music rests
  and a lowest-priority soft voice otherwise; the PS1 has 2 dedicated bed
  voices.
- Beds keep going when the music rests (5.12), duck under music peaks and
  rise as the music thins.

### 5.11 Leitmotif quoting system

- **Library:** each motif is stored once (head, tail, rhythm, preferred
  harmony, forms per dial): 16-40 bytes each.
- **Quote slots** in nodes: COUNTER bars (B sections), FILL (the last 2
  beats of a phrase), INTRO/OUTRO bars, BASS ostinato (augmentation),
  DESCANT (above the lead), STINGER (a sting replaced by a motif head).
- **Triggers (context → motif):** a character within N cells; the active
  quest giver; the story beat; an item held (Pip's bunny); **the speaker in
  a dialogue** (their motif in the bass while they talk, babble on top); the
  pet following (a BEEP descant at 1 in 4 phrase ends); doors and places;
  unlocks.
- **Transform:** transpose to the current key; map the mode by scale degree
  (unless the motif is fixed-mode, like WALL); fit the rhythm (× 2 or × ½,
  clip the tail); take the instrument from the current dial; counter quotes
  sit 3 dB under the lead, FILL quotes are lead-level.
- **Cooldowns and priority:** at most 1 quote per 4 bars overall, and the
  same motif at most once per 16 bars. Priority: story beat > speaker >
  quest > proximity > pet. A cue may forbid quotes (boss phases except
  scripted ones).
- **Infection (Ch.2), a "negative quote":** the Hook's tail gets Vex's droop
  with probability `infect`/255 at each phrase end; the harmony detunes by
  `infect`/16 cents; a ♭5 pedal fades in at `infect` ≥ 192.
- **Hot/cold searches:** a harmony voice detunes with the distance to the
  target and locks in when close.
- **True names:** generated per resident id (2.10), quoted in exorcisms,
  apartments and the credits.

### 5.12 Rests and fatigue

- Every cue has a **rest policy**. After K minutes of continuous play
  (overworld 3-4 min, menus 2 min, combat never) the music drops to the bed
  at a phrase end for 16-32 bars ("the world breathes"), then re-enters
  through an INTRO or a different node.
- **Idle:** the music thins to I1 after 20 s of no input and rests after 90 s.
- **Re-entry:** after a rest the graph starts on a non-anchor node half the
  time and on a different key variant a quarter of the time.
- **Session memory:** the arranger remembers the last N nodes per cue across
  scene switches (in RAM, not saved), so re-entering an area does not replay
  its intro every time.
- Option **AU13** (Location music: Auto / Jukebox playlist / Silence) and
  **AU20** (Mute in menus) apply here.

### 5.13 Seeded determinism

- **Seeds:** `cue_seed = hash(town.seed, day, cue_id, chapter)`; per pass,
  `hash(cue_seed, pass, node, role_salt)`. **Separate RNG streams per role**
  (melody, harmony, bass, drums, quotes, beds).
- **Integer only:** no floats. Timing is in **microticks (1/240 s)** with
  fixed-point tempo accumulators, the same idea as today's 1/192-note tick
  accumulator.
- **Contract:** the same save and inputs give the same note events on
  native, GBA and PS1. Tiers and the governor change only *rendering*
  (which roles are audible, the mix rate), never RNG consumption or event
  generation.
- **Tests:**
  1. A note-event hash per cue under scripted driver curves (native, like
     `audio_note_count`).
  2. Graph properties over 1,000 picks: no immediate repeat, the bag
     guarantee holds, an anchor at least every 3 picks.
  3. The repetition metric (5.14) on simulated 10-minute and 1-hour walks.
  4. Stingers land on their grid with 0 microticks of error.
  5. A full baby scratch on DECK returns to the exact song position.

### 5.14 Acceptance targets

| Metric | Target |
|---|---|
| exact repeat of an 8-bar span (all roles, after variation) | none within 10 min of one cue at a constant game state |
| exact repeat of a 32-bar span | < 1 % chance per hour |
| theme recognisability | the head motif at least once every 3 phrases |
| audible loop seam | none: every node boundary is a legal cadence or a generated turnaround |
| scratches / stings outside DJ contexts | ≤ 1 scratch per 8 bars, ≤ 1 sting per bar |
| stinger timing | exact to the microtick on the GBA, PS1 and native |

---

## 6. Record scratches (first class, on both consoles)

### 6.1 The model: a virtual turntable

- **State:** playhead `P`, velocity `V` (Q8.8 fixed point, +256 = normal
  speed, negative = backwards), crossfader `F` (0..255), crackle `C`,
  friction `Z`.
- **A scratch pattern** is a list of segments in musical time (1/48-beat
  units at the current tempo; absolute ticks when there is no tempo). Each
  segment has a start and end `V`, a curve (linear, ease, sine, exp), a
  fader action (open, closed, **chirp** = closes at the velocity peak), hand
  jitter (seeded ±% on `V`) and a crackle boost.
- **Rendering rules:** the source is read at `P` with step `V`; gain =
  min(1, 2|V|) × F (a slow record is quiet); when |V| < 0.5 the highs are
  cut (GBA soft one-pole; PS1 muffled wave banks). A **friction "zip"**
  (10-25 ms of band-passed noise) fires on every direction change, and a
  drag noise runs while |V − 1| > 0.3.
- **After the pattern**, one of: **RESUME** (P jumps to where the song would
  be, beat-locked: "the DJ catches the beat"); **CONTINUE** (the song goes
  on from P, as after REWIND); **STOP** (silence until resumed); **NEXT_CUE**.

### 6.2 Sources: what gets scratched

| Source | What it scratches | How | Cost | Where |
|---|---|---|---|---|
| **DECK** | the music itself, every voice (PSG, soft and SPU) | the arranger scrubs its own position: pitch × \|V\|, envelopes advance by \|V\|, notes re-trigger in reverse order when P crosses their starts backwards (from a 2 s event log), voices gate silent at \|V\| < 1/16, drums re-trigger with scaled noise clocks / sample pitch | ~0 CPU, 1 KB event log | **everywhere**, including 3D combat |
| **TAPE** | the *actual recent output* incl. the babble (GBA: also SFX) | a ring buffer of the last 0.3-1.5 s is frozen and played back with `V` and linear interpolation | GBA 0.2-0.4 % to record, 2.7 % while scratching; PS1 6.5 % to record (armed), 2 % while scratching | 2D scenes, cutscenes, menus, Studio, Concert |
| **SAMPLE** | a chip-choir "aah" (formant synth, not a real voice), a babble chop of the current speaker, a pre-rendered babble "Make friends!" | a one-shot played with `V` | a voice | DJ moments, menu stings |

**Hybrid default:** TAPE for the grit and the exact smear, DECK underneath
at −9 dB for body. **SFX are not scratched on DECK**: the world keeps
playing while the music gets scratched, which is part of the joke.

| Context | Source |
|---|---|
| Comic stings in cutscenes, menus, 2D scenes | TAPE + DECK |
| Combat and 3D | DECK only |
| Music transitions | DECK (REWIND continues from P) |
| Studio / Concert DJ | TAPE (the song's own last 1-1.5 s) + SAMPLE |
| Death | DECK STOP |

### 6.3 Pattern library

Lengths are at the current tempo. "Fader": `o` open, `x` closed, `c` chirp.

| ID | Name | Length | Velocity path (V, 1.0 = play speed) | Fader | Typical use |
|---|---|---|---|---|---|
| SCR_BABY | Baby scratch | ½ beat | 0 → +2.4 → 0 (sine, ¼), 0 → −2.4 → 0 (¼) | o | DJ basics, quick stings |
| SCR_FORWARD | Forward / stab | ¼ beat | 0 → +3 → 0 (⅛), back stroke (⅛) | o then x | punctuation |
| SCR_CHIRP | Chirp | ¼ beat | push 0 → +2.6, pull −2.6 → 0 | c | cute "bird" gags, the pet |
| SCR_TRANSFORMER | Transformer | 1 beat | 0 → +1.2 → 0 (½), 0 → −1.2 → 0 (½) | 1/32 on-off gating | robots, Rivet, the factory |
| SCR_TEAR | Tear | ½ beat | forward in two humps (0 → 2.2 → 0.4 → 2.0 → 0), back in one | o | dramatic rips |
| SCR_FLARE | 1-click flare | ½ beat | as baby | one 1/64 click closed mid-stroke | DJ flair |
| SCR_SCRIBBLE | Scribble | ½ beat | ±1.3 alternating every 1/48 (P barely moves) | o | nervous "brrrr" |
| SCR_WIKKA | Wikka-wikka | 1 beat | 4 fast babies (⅛ each), rising amplitude | o | DJ hype, Concert |
| SCR_REWIND | Rewind / pull-up | 1 beat + snap | +1 → −4 over ¾ beat (exp), hold −4 for ¼; P lands 1-2 bars back; **snaps to +1 on the next downbeat** | o | transitions, retries, flashbacks |
| SCR_SPINBACK | Backspin | 1 bar | −6 decaying exponentially to 0 | o | phase changes, boss deaths, song ends |
| SCR_STOP | Full stop / brake | ½ beat | +1 → 0 (exp): the pitch drops about 2 octaves | o, then silence | "power down", deaths, freeze frames |
| SCR_NEEDLE | Needle drag | ¼ beat | +1 → +3 → 0 + accelerating groove clicks + a pop | o, then silence | **the** comedy record scratch ("wait, WHAT?") |
| SCR_STUTTER | Loop roll | ½ beat | jump back 1/16 four times | o | spit takes, glitches, a hurt boss "cough" |
| SCR_WOBBLE | Warped record | continuous | 1 + 0.06·sin(1 Hz) + slow drift | o | haunted house, dreams, the sick pet (3 %) |
| SCR_SLOWMO | Half speed | n beats | 1 → 0.5 in ½ beat, hold, → 1 | o | slow-motion comedy |

### 6.4 Vinyl crackle and friction

- **Crackle:** Poisson impulses (2-60 per second); each impulse is 1-3
  samples of seeded random amplitude with a tiny one-pole ring (the "pop
  body"); 1 in 40 is a big pop; under it a low hiss. **Rotation tick:** an
  optional click every 1.8 s (one turn at 33⅓ rpm) for the Dream House radio
  and the dreams.
- **Needle drag:** groove-crossing clicks accelerating from 20 to 200 per
  second with falling pitch, then a pop and silence.
- **GBA:** a soft crackle kernel (about 5 cycles per sample, pure synthesis,
  no RAM).
- **PS1:** a pre-rendered 2 s crackle ADPCM loop (16 KB SPU RAM), NON
  hardware noise for the hiss, and zip one-shots (4 KB) pitched per event.

### 6.5 Where scratches are used

- **Comic stings** (5.9): `CE_RECORD_SCRATCH`, `CE_FREEZE`, `CE_REWIND`,
  `CE_STUTTER`, `CE_SLOWMO`.
- **Menus ("wait, WHAT?"):** selecting Monster Mode, the first "Lay to
  Rest", deleting a save, the "I AM A MONSTER" confirmation, catching the
  title screen's idle corruption.
- **Deaths:** SCR_STOP (power down).
- **Transitions:** the invasion morph (NEEDLE + silence), boss phase changes
  (SPINBACK), Boss Rush links (REWIND), retries (REWIND), episode recaps
  (REWIND), cliffhangers (STOP), Dream House state flips (REWIND).
- **DJ:** the Studio scratch lane, the Concert DJ booth and battles, the
  Jukebox's "Scratch it!".
- **Diegetic:** the Dream House radio (crackle bed), dreams and the haunted
  house (WOBBLE), the sick pet (WOBBLE 3 %), the credits fast-forward
  (V = 1.5).
- **Accessibility:** a **Gentle audio** option replaces scratches with
  1-beat fades and caps sudden stinger loudness.

### 6.6 Player-placed scratches (Studio and Concert)

- **Format (append-only, zero bytes):** a drum-track cell holds a
  `SONG_DRUM_*` value (0..8) in bits 0-3. **Bits 4-7 hold a scratch pattern**
  (0 = none, 1..15). `DoomoSong` stays 892 bytes and the `'SONG'` chunk is
  unchanged; old songs have 0 there. **`song_sanitize` must mask the low
  nibble** instead of rejecting cells ≥ `SONG_DRUM_COUNT`, and the drum player
  must read `cell & 15`.
- **Scratch ids:** 1 baby, 2 forward, 3 chirp, 4 transformer, 5 tear, 6
  flare, 7 scribble, 8 wikka, 9 rewind (1 bar back), 10 spinback, 11 stop,
  12 needle, 13 stutter, 14 crackle toggle (vinyl bed on/off from here on),
  15 slow-mo (half speed until the next scratch cell).
- **Semantics:** a scratch starts on its step (16th) and lasts its pattern.
  The source is TAPE (the song's own last ~1 s: it *is* a record) where
  available, otherwise DECK. Afterwards the song resumes beat-locked, except
  that REWIND continues from P and STOP holds silence until the next
  note-bearing step.
- **Studio UI:** on the drum track in NOTES, SELECT toggles the scratch layer
  (cells show a tiny record); A cycles patterns; the preview plays it on the
  current song. The composer adds 1-2 scratches per pattern to Lo-fi Beats,
  Hardcore Punk, Industrial and Synthwave songs ("Hellvile Flow" gets them
  by default). Unlock idea: **"Wikka Wikka"** for placing 10 scratches.
- **Concert DJ booth** (`CMODE_DJ`): the DJ AI picks patterns by personality
  and genre; DJ battles; **Take the decks** controls:
  - **GBA:** hold R = hand on the record (the deck follows you, not the
    motor); D-pad up/down = push/pull (velocity ramps while held; release
    and the motor catches the beat back); A = fader open while held; B =
    cut; L = backspin gesture.
  - **PS1:** the same digital mapping. With a DualShock in analog mode the
    **left stick's vertical axis is the platter** (stick position → playhead
    offset, its rate of change → V); R1 = fader, L1 = cut, R2 = backspin.
  - **Scoring:** pattern prompts on the beat grid, scored on the timing of
    direction changes and fader clicks against the target pattern (like the
    exorcism prayer rhythm).

### 6.7 GBA scratch implementation and budget

- **DECK:** done in the core arranger; the platform receives pitch-scaled
  voice commands at the 240 Hz microtick. PSG frequency registers change
  without retriggering (smooth); soft voices scale their step per block.
- **TAPE ring:** signed 8-bit mono, written by the mixer's output pass at
  `mix_rate / D` (the average of D samples of L+R), **8 KB borrowed from
  `gba_big`** while no 3D view is up (tiers S and S+): 1.35 s at 6,052 Hz
  (18,157 / 3) or 1.56 s at 5,256 Hz (21,024 / 4). **There is no resident
  ring:** 3D scenes (tiers L and M) scratch with DECK + crackle, which keeps
  the resident EWRAM cost down (7.6).
- **Ghost PSG:** the PSG is analogue hardware and never passes through the
  mixer, so a ring of the DirectSound mix alone would miss most of the
  music. While **armed** (cutscenes, menus, 2D scenes, and ≤ 2 beats before a
  scheduled scratch transition; never in 3D combat), the mixer also renders
  a cheap approximation of the 4 PSG channels from their shadow registers
  (frequency, duty, volume, wave bank, LFSR) **into the ring only**, never to
  the FIFOs. About 14 cycles per ring sample: 0.6-1.2 % CPU while armed.
- **Tape voice:** a soft voice reads the frozen ring with a signed 16.16
  step and linear interpolation (about 20 cycles/sample), plus the one-pole
  low-pass when |V| < 0.5. The PSG music mutes at the next microtick, or is
  DECK-scrubbed at −9 dB for the hybrid.
- **Budget (tier S, 18,157 Hz):** tape voice 20 × 304 + crackle 5 × 304 =
  7,600 cycles per vblank = **2.7 % CPU, only during the scratch** (the
  muted soft music voices free about as much). Ring recording, always on in
  tier S: 0.3-0.4 %. RAM: 1 KB DECK log resident; the 8 KB ring is borrowed
  in tier S only.

### 6.8 PS1 scratch implementation and budget

- **DECK:** each microtick (240 Hz root counter) every music voice's SPU
  pitch register is set to pitch × |V|. The 14-bit pitch register tops out
  at 0x3FFF (×4); a voice that would exceed it switches to its 28-sample
  short cycle. Reverse event replay works as on the GBA. The SPU ADSR cannot
  run backwards, so when V < 0 notes re-key with a short attack and a scaled
  sustain. **The reverb keeps running, so scratched notes leave real smeared
  tails.** Volume follows min(1, 2|V|).
- **TAPE:** while **armed** (2D scenes, cutscenes, menus, Studio, Concert),
  a CPU **shadow render** of the music and babble runs at 11,025 Hz, mono,
  8-bit. Each frame's 184-byte block is DMA'd into a **16 KB ring in SPU RAM
  (1.5 s)**: the SPU can only *play* ADPCM, but its RAM is plain memory the
  CPU can write and read back by DMA (8.5). To scratch, the CPU reads back a
  ~2 KB window around the playhead each frame, resamples the frozen ring with
  V (linear interpolation) and encodes SPU ADPCM (fast mode) into a **2 ×
  224-byte stream double buffer** in SPU RAM:
  14 blocks = 392 samples, about 2 frames per half. One stream voice loops
  the buffer; the SPU IRQ address flips between the halves (the PSn00bSDK
  streaming pattern) and the IRQ refills the other half. Latency is at most
  2 frames, absorbed by the scheduler's pre-roll.
- **Capture buffers (evaluated, used for one effect):** the SPU's capture
  areas record only CD L/R and **voices 1 and 3**, each as a 1 KB ring (512
  samples = 11.6 ms), so they cannot capture the whole mix. But babble
  already lives on SPU voice 1 (plat channel 1 → voice 1), so in Concert and
  Studio DJ mode the **real sung babble**, with its SPU ADSR and PMON
  vibrato, is drained by SPU IRQs at the capture half-points (DMA read,
  decimate × 4 → 11 kHz, DMA write) into the same SPU-RAM tape ring and
  scratched as a SAMPLE: **"scratch the singer"**.
- **Crackle:** the SPU crackle loop + NON hiss on one voice (the global noise
  clock is set for hiss while vinyl is active and is otherwise free for wind
  and risers) + zip one-shots.
- **Freeze:** for `CE_FREEZE` all EON reverb inputs are cut and the reverb
  feedback gains are raised (kept below unity), so the last chord hangs as a
  shimmering tail; they are restored on resume.
- **Budget:** DECK ~0.2 % (pitch writes); the shadow render 8 voices ×
  11,025 Hz × ~25 cycles = 2.2 M cycles/s = **6.5 % CPU while armed**
  (only in 2D scenes, where the PS1 holds 30 fps with headroom; never armed
  in combat or exorcism, which are CPU-bound); the TAPE encode ~60 cycles ×
  11,025 = 0.66 M = **2 % during a scratch**; the capture drain ~1.3 %
  (Concert/Studio DJ only). Main RAM: a 2.5 KB staging / read-back window +
  1 KB DECK log; the 16 KB ring is in SPU RAM.

---

## 7. GBA: pushing the hardware

The plan is all four PSG channels used like the best GBA drivers, **plus**
a stereo DirectSound soft synth in IWRAM ARM code. In 3D scenes it costs
the same CPU as today's mixer does at its worst; in 2D scenes it spends
real CPU to sound huge.

### 7.1 PSG: every channel, every trick

| Channel | Main roles | Tricks |
|---|---|---|
| **Pulse 1** (sweep) | lead, or kicks, lasers, slide whistles and risers | **hardware frequency sweep** for kick "thumps", STG_SLIDE_UP/DOWN, the boing, bird chirps and lasers, smoother than any 30 Hz update and free; **duty switching** at microtick rate (12.5 → 25 → 50 → 25) as pseudo-PWM, and 25 ↔ 75 % flips against pulse 2 for a phasey chorus in stereo; decays via the **hardware envelope** (no retrigger clicks) |
| **Pulse 2** | harmony, counter, arps; the echo-by-repeat partner of pulse 1 | **super-fast arps**: 3-4 notes per frame at the 240 Hz microtick give a shimmering chip chord; ping-pong panning via the PSG L/R enable bits per 8th note |
| **Wave** (ch3) | bass, organ, "power chord", vowel formants | **64-step mode** (both banks as one cycle): finer waves, e.g. a **one-channel power chord** (2 cycles of the root + 3 of the fifth in 64 steps, tuned an octave below the root), drawbar organs, **vowel-formant cycles** (a "talking bass" and the choir's vowels, matching the PS1's vowel ADPCM). **Bank swapping**: write the idle 32-step bank, flip at a microtick → **morphing timbres** (tri → saw → square = a "filter sweep" on a channel with no filter; a growly Sunsoft-style bass). **Volume 0/25/50/75/100 % without retrigger** → tremolo and fades. **4-bit "PCM-ish"** bank streaming at ≤ 2 kHz for rumbles and thuds |
| **Noise** (ch4) | hats, snares, crashes, risers; metallic hits | 15-bit LFSR with a per-hit clock divider and shift (brightness round-robin); **7-bit periodic mode** for pitched metal: Rivet's anvil, bees, the LCD crackle, gritty demon bass; **shift sweeps** for crashes and risers |
| Global | | `SOUNDCNT_L` master volume L/R (3 bits each) to **duck the PSG** under dialogue and stingers; per-channel L/R enables for hard panning; PSG at 100 % in `SOUNDCNT_H` |

**Write discipline:** frequency changes never retrigger (slides and DECK
scrubbing stay smooth); envelope writes happen only at note-on (instrument
decays map to hardware envelope steps); duty writes are free. Zombie-mode
volume tricks are **not** used, because GBA models and emulators differ.

### 7.2 The DirectSound soft synth

- **Stereo:** FIFO A = left, FIFO B = right (`SOUNDCNT_H` routing), both
  clocked by timer 0; DMA 1 → FIFO A, DMA 2 → FIFO B. Today only FIFO A is
  used, at 10,512 Hz.
- **Packed-stereo accumulation:** each mono voice is added as
  `acc += sample × (volL | volR << 16)` with a single `MLA`; the output pass
  unpacks `L = (int16)acc`, `R = (acc − L) >> 16`, which is exact while each
  side fits 16 bits. That gives **true per-voice panning and volume at the
  cost of a mono voice.**
- **Blocks:** the vblank mix (as today) is split at the 4 microtick
  boundaries per frame, so soft notes start sample-accurately. Kernels are
  4×-unrolled ARM in IWRAM.
- **Kernels** (estimated cycles per output sample, accumulate traffic
  included; to be verified with the harness profiler, `profile N --nm`):

| Kernel | Cycles | Notes |
|---|---:|---|
| output pass (unpack, master, clip, 2 FIFO bytes) | 12 | per stereo sample, always |
| PCM, ROM source, nearest | 14 | drums, vocal chops, SFX (`ldrsb` from ROM ~6 cycles) |
| PCM, RAM source + linear interpolation | 20 | the tape voice, decoded chops |
| **pulse with true PWM** | 7 | phase accumulator vs a duty threshold swept per block by an LFO / envelope |
| saw | 7 | |
| **hard-sync saw** | 10 | the metal lead |
| triangle / stepped 4-bit triangle | 9 | 16-entry IWRAM table |
| wavetable (64-step frame in IWRAM) | 9 | +5 for a 2-frame morph crossfade |
| noise (LFSR + clock divider) | 10 | |
| 2-op FM-lite (sine table) | 16 | tier S only: bells and growls |
| + one-pole LP / HP | +3 | muffles, "behind the door", the scratch dulling at \|V\| < 0.5 |
| + ring mod / bit-crush hold | +1 / +2 | |
| echo (2 taps, half-rate line) | 7 | tier S |
| crackle | 5 | |
| ring record (decimated) / + ghost PSG (armed) | 3 / +14 | tier S |
| per voice per vblank (envelope, LFO, setup) | 150 | per voice |

- **Voice priority and stealing:** SFX > tape > lead/bass > drums > counter
  > pad > bed. The arranger hands over every role; the platform renders in
  priority order until its tier's budget is spent (7.4).
- **Speaker mode** (AU10): the output pass folds (L + R) / 2 into both sides
  and the arranger swaps the bass cycle for its harmonic-rich speaker
  variant.

### 7.3 CPU budget: how many voices fit at which mix rate

A frame is **280,896 cycles**, so **1 % CPU = 2,809 cycles per vblank**.
The rates are the classic vblank-exact ones (an integer number of samples
per frame):

| Rate (Hz) | Timer reload | Samples / vblank | Base (output) | One PCM voice | One chip voice (≈ 8 c) | Echo | Ring |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 10,512 | 1,596 | 176 | 0.75 % | 0.93 % | 0.55 % | 0.44 % | 0.19 % |
| 13,379 | 1,254 | 224 | 0.96 % | 1.17 % | 0.69 % | 0.56 % | 0.24 % |
| 18,157 | 924 | 304 | 1.30 % | 1.57 % | 0.92 % | 0.76 % | 0.32 % |
| 21,024 | 798 | 352 | 1.50 % | 1.81 % | 1.06 % | 0.88 % | 0.38 % |

**Maximum voices** at each budget (on top of the always-free 4 PSG
channels):

| Rate | Budget | Max PCM-only | Max chip-only | Recommended mix |
|---|---:|---:|---:|---|
| 10,512 | 6 % (3D combat) | 5 | 9 | 4 SFX + 2 chip (lead double, PCM kick or crackle) + DECK |
| 13,379 | 9 % (overworld) | 6 | 11 | 4 SFX + 4 chip, no echo (echo-by-repeat instead) |
| 18,157 | 16 % (2D scenes) | 8 | 14 | 4 SFX + 8 chip + echo + ring |
| 21,024 | 20 % (Studio, Concert, Jukebox) | 9 | 16 | 2 SFX + 2 PCM (drums, chops) + 9 chip + echo + ring |

**Today's baseline:** the current C mixer (4 SFX voices at 10,512 Hz, about
20 cycles per voice-sample with `int16` accumulator traffic, plus the output
pass) costs about **5.5 % when four SFX play**, and about 0.3 % when idle.

- **Tier L (3D) ≈ 5.6 %:** 0.75 + 4 × 0.93 + 2 × 0.55 + 0 (no ring in 3D,
  DECK only), which matches today's worst case: the ARM rewrite pays for the
  2 new voices, so **3D fps does not change**.
- **Tier M (overworld) = 6.1-8.4 %:** 0.96 + 4 × 1.17 + 4 × 0.69 = 8.4 %
  with 4 SFX, but there are rarely more than 2 SFX on the overworld, so
  6.1 % typical. That is +0.3-1.6 ms per 50 ms frame; the overworld (r3d
  36.2 ms, 21.7 fps) stays under the 50 ms line.
- **Tier S (2D) = 14-20 %:** a 2D frame at 20 fps has 842,688 cycles (3
  vblanks); audio at 20 % leaves about 40 ms of each 50 ms for the scene.
  The **UI ≥ 20 fps** target is guarded by the governor (7.4).
- **Sequencer + microtick IRQ:** the 240 Hz handler (about 150 cycles) adds
  0.2 %; the arranger runs in the logic tick (well under 1 % per tick:
  bounded bytes per role, as today).

### 7.4 Audio LOD tiers and the governor

| Tier | Scenes | Rate | Soft voices | FX | Scratch source |
|---|---|---|---|---|---|
| **L** | 3D views: combat, Hell Breaches, arena, invasion encounters | 10,512 | 4 SFX + 2 music | none (echo-by-repeat) | DECK + crackle |
| **M** | the story overworld and island hub (3D/2.5D), Dream House (if its renderer holds `gba_big`) | 13,379 | 4 SFX + 4 music | none | DECK + crackle |
| **S** | 2D scenes: menus, cutscenes, credits, pet, exorcism, minigames, fishing, cooking, library, dreams, workshop, resident screens | 18,157 | 4 SFX + 8 music | echo, ring, ghost PSG | TAPE + DECK |
| **S+** | Studio, Concert, Jukebox | 21,024 | 2 SFX + 11 music (2 PCM + 9 chip) | echo, ring, crackle | TAPE + DECK + SAMPLE |

- **Governor:** it measures each frame with `gba_cycles()`. If a 2D scene's
  rolling 8-frame average passes 47 ms, or a 3D scene runs more than 1 ms
  over its tier's audio budget, it steps down (fewer music voices first,
  then a lower rate at the next phrase boundary). It steps back up after 4 s
  of headroom. Because tiers only change *rendering*, the governor never
  touches determinism.
- **Option AU21** (GBA mixing rate: 10 / 13 / 18 kHz) caps the tier.

### 7.5 Timing: a sample-locked music clock

- Today the sequencer runs in the 30 Hz logic tick. When a 3D frame takes
  50-66 ms the logic catches up with 2 ticks in one frame, so **notes bunch
  up** (at 184 bpm a 16th note is 2.4 ticks, which is already uneven).
- **New:** the arranger stays in the logic tick (deterministic and testable)
  but writes **timestamped events one or two logic ticks ahead** into a
  queue. The platform plays them on a **240 Hz microtick clock**: **timer 1
  cascades off timer 0**, so its IRQ fires every N samples (44 / 56 / 76 /
  88 at the four rates), which is exactly **4 microticks per vblank, 238.9
  Hz** at every rate, locked to the audio.
- **Latency alignment:** the DirectSound block mixed at vblank N plays
  during frame N + 1, so **PSG events are applied 4 microticks late** to land
  together with the soft voices.
- **IRQ nesting:** a tier S+ mix is ~3.4 ms inside the vblank IRQ; enable
  nested IRQs during the mix (system mode) so microtick PSG writes are not
  delayed by up to 3.4 ms.

### 7.6 RAM plan (tight on purpose)

EWRAM has 7.5 KB free today and integration is working toward more
headroom; IWRAM is full (6 KB stack reserve, 1,744 B measured high water).
So **the big audio buffers are borrowed, not owned**.

| Item | Size | Where | When |
|---|---:|---|---|
| FIFO double buffers, stereo, sized for 13,379 Hz | 896 B | EWRAM, resident (today: 352 B mono) | always |
| Soft voice states (8 × 40 B) | 320 B | EWRAM, resident | always |
| Event queue (2 logic ticks) + DECK log (2 s of events) | 1,024 B | EWRAM, resident | always |
| Arranger state (graph positions, RNG, params, stems, session memory) | ~500 B | EWRAM, resident | always |
| **Resident total** | **≈ 2.7 KB** | (net +2.4 KB vs today's mixer) | |
| FIFO for 18/21 kHz (+512 B), 8 more voice states (+320 B) | 0.8 KB | **borrowed from `gba_big`** | tier S, S+ |
| TAPE ring | 8 KB | borrowed from `gba_big` | tier S, S+ |
| Echo line | 4 KB | borrowed from `gba_big` | tier S, S+ |
| Morph wavetable frames, decoded chop cache | 1-2 KB | borrowed from `gba_big` | tier S, S+ |
| IWRAM: lean kernels (PCM, pulse/PWM, saw, tri, noise, output, packed stereo) | ~1.5 KB code (replaces ~0.5 KB) | IWRAM | always |
| IWRAM: rich kernels (sync saw, FM-lite, morph, filters, tape, ghost PSG) | ~2.5 KB code | an **IWRAM code-overlay window** shared with the 3D renderer's IWRAM loops, loaded on 2D scene entry | tier S, S+ |

- **Borrowing `gba_big`:** it is a renderer cache and the save window,
  claimed with `gba_big_claim()`. With no 3D view up its renderer use is
  dead, so audio claims it. A save or a 3D view reclaims it, and audio then
  falls back to tier M features: the ring is dropped (scratches become DECK)
  and echo becomes echo-by-repeat. Nothing breaks.
- **Finding +1 KB of IWRAM for the lean kernels:** the flash chip routines
  (`flash.iwram.c`) only run inside saves, so they can be **copied onto the
  stack at save time** (IWRAM, fast; ~1 KB is well inside a 6 KB stack with
  a 1.7 KB high-water mark). That frees their permanent IWRAM.
- **The IWRAM code-overlay window** mirrors what the PS1 already does for
  scene code. The 3D renderer's hot loops are dead in 2D scenes; making
  them a swappable IWRAM overlay gives the rich audio kernels room in
  exactly the scenes that can afford them. If this is not done, tier S uses
  the lean kernels only (still PWM, PCM, saw, tri, noise and echo).

### 7.7 ROM plan

The cartridge is 2.4 MB of a possible 32 MB, so audio may use **up to 1.5 MB
of ROM**:

| Asset | Size |
|---|---:|
| 6 drum kits (Toy Box, Bone Rattle, Boom 808, Plush Pillow, Hell Forge, Classic), 8-bit at 8-16 kHz, 2-3 round-robins | ~120 KB |
| chip-choir vowels, babble formant chops, "aah" SAMPLE scratch sources | ~150 KB |
| wavetable morph frames (64-step) and wave-channel cycles | ~32 KB |
| crackle / friction one-shots (the generator synthesises most of it) | ~8 KB |
| area one-shots (anvils, factory, soda, bells, organ) | ~120 KB |
| score: all cues tokenised (section 10) | ~150-250 KB |
| motif library, groove DNA, reharm pools | ~10 KB |

---

## 8. PS1: the whole SPU

The SPU mixes 24 voices in hardware, so the CPU cost of music is tiny. The
PS1's limits are main RAM (with overlays, about 75 KB above the 64 KB stack
reserve) and SPU RAM (512 KB, mostly free). The plan uses every SPU feature
and keeps main RAM nearly untouched.

### 8.1 Voice map (24)

| Voice(s) | Role | Notes |
|---|---|---|
| 0 | LEAD | `plat_tone` channel 0, as today |
| 1 | HARM + **BABBLE** | **captured by the hardware** (capture area 0x0800): "scratch the singer" (6.8) |
| 2 | BASS | |
| 3 | KIT noise (the 4-channel "core quartet") | also captured (0x0C00); spare capture for a future effect |
| 4-5, 6-7, 8-9 | **PMON pairs** (modulator → carrier): FM bells, growls, vibrato LFOs | voice *n* is modulated by voice *n* − 1 |
| 10-15 | music stems: counter, pads, PWM saw pair (2), choir (2) | |
| 16-20 | SFX (5) | SFX may steal music stems by priority when > 5 play |
| 21 | TAPE stream voice | ADPCM stream double buffer |
| 22 | bed / vinyl hiss (**NON** hardware noise) | the global noise clock belongs to this voice |
| 23 | bed 2 / extra stem | |

Today SFX use voices 4-23 dynamically. The new map gives music 16 voices
and SFX 5 dedicated voices plus stealing. Combat SFX bursts can still take
up to 12 voices by stealing pads, choir and counter first.

### 8.2 Waveform banks and SPU tricks

- **ADPCM chip banks** (single-cycle, a long 112-sample and a short
  28-sample cycle each, as the current driver does): **16 pulse duties**
  (1/32 .. 16/32), saw, smooth and 4-bit-stepped triangle, sine; **muffled
  variants** of each (pre-low-passed: the PS1's "filter", because the SPU
  has none); **power-chord cycles** and **vowel cycles** (aah / ooh / ee, to
  match the GBA's wave channel); noise loops in several colours.
- **Morph loops:** 32-cycle loops whose duty or brightness changes cycle by
  cycle: **baked PWM and filter sweeps** that cost zero CPU (one 4,096-sample
  loop ≈ 2.3 KB). Plus **baked 2-op FM loops** (bells, e-piano-ish chip
  keys, growls), still bit-crunchy.
- **Repeat-address switching:** waves are encoded without the loop-start
  flag, and the voice's repeat address register is rewritten so the next
  loop jumps to another cycle. That gives **glitch-free stepped PWM / filter
  morphs at cycle boundaries**, driven at the microtick. *Verify on hardware
  and PCSX-Redux: a block with the loop-start flag overwrites the repeat
  address, so the bank must be encoded without it.*
- **Inverted detuned saw pair = continuous PWM:** two saws at the same pitch
  with one at negative volume, detuned a few cents. Their sum is a pulse
  whose width sweeps on its own: classic analogue PWM from two SPU voices.
- **PMON (pitch modulation):** voice *n*'s pitch is modulated by voice *n* −
  1's output. An audio-rate modulator gives **real FM-ish timbres**; a slow
  sine modulator gives vibrato with no CPU. *Verify that the modulator can be
  silenced by its L/R volume (it should, because modulation reads the
  enveloped output before the volume stage).*
- **NON hardware noise:** one voice; the noise clock (SPUCNT) is global, so
  it is reserved for live sounds whose pitch sweeps (vinyl hiss, wind,
  risers, vape hiss). Drums use **noise ADPCM loops at per-voice pitch**,
  which is the SPU's version of the GBA's noise clock divider.
- **Hardware ADSR:** the core's instrument envelopes (attack, decay,
  sustain, release) map to SPU rates through a table: smooth, zipper-free,
  zero CPU. **Sustain-increase mode** gives brass and organ swells.
- **Volume sweeps:** linear and exponential L/R sweep modes provide
  **auto-pan, fades and ducking in hardware**.
- **Key-on batching:** KON/KOFF bitmasks are written once per microtick, so
  chords start together.
- **No CD-DA / XA music.** The PS1 renders the same sequenced score as the
  GBA in real time, which keeps it interactive, scratchable and at parity.
  The CD is used only to stream **area banks** into SPU RAM when an overlay
  group loads (it shares the existing ~0.3 s overlay read).

### 8.3 Reverb per area

The reverb work area sits at the top of SPU RAM, reserved at **0x18040
bytes** so every standard preset fits. **Reverb runs at 22.05 kHz
internally, which is chip-friendly.** Preset changes happen only in silence
(mute the reverb output, set the base, clear the area with DMA, enable). The
depth and the per-voice sends (EON) change freely.

| Area / cue | Preset (work-area size) | Send | Notes |
|---|---|---|---|
| HaHaland day, shops, apartments | Room (0x26C0) | low | |
| Rain | Studio Medium (0x4840) | low-mid | |
| Snow, night, the church, the graveyard | Hall (0xADE0) | mid | |
| Concert: Sugar Bowl / Disco Inferno / Graveyard / Hellfire / Dreamland | Studio Small (0x1F40) / Hall / Space Echo (0xF6C0) / Room / Echo (0x18040) | per stage | |
| Hellvile, Crypt Row, Vape Factory Ruins | Space Echo | mid | |
| Combat (all Hell Breaches) | Room | **low** (clarity) | Bone Cathedral: Hall, mid |
| Dreams, nightmares, the haunted house | Echo / Half Echo (0x3C00) | high | WOBBLE + echo = lost |
| Freeze frame (`CE_FREEZE`) | the current preset, feedback raised (below unity) | inputs cut | the "reverb freeze" |

Option **AU11** (PS1 SPU reverb: Off, Room, Studio, Hall, Space echo) and
**AU12** (reverb amount) cap this table.

### 8.4 Timing

A **root counter IRQ at 240 Hz** consumes the same timestamped event queue
as the GBA (7.5): pitch writes (incl. DECK scrubbing), KON/KOFF batches,
repeat-address morph steps and volume sweeps. The handler is short; music
costs about **1-2 % CPU** in total. The arranger runs in the logic tick, as
on the GBA.

### 8.5 SPU RAM map (512 KB)

```
0x00000-0x00FFF   capture buffers (hardware: CD L/R, voice 1, voice 3)          4 KB
0x01000-0x01FFF   silent loop + core chip waves (today)                          4 KB
0x02000-0x0BFFF   chip banks: 16 duties x 2, saw/tri/sine, muffled variants,
                  power-chord + vowel cycles, morph loops, FM loops              40 KB
0x0C000-0x19FFF   6 drum kits (+ round-robins)                                    56 KB
0x1A000-0x23FFF   chip choir (vowel loops, SATB), babble formants, "aah"         40 KB
0x24000-0x27FFF   vinyl crackle loop + zip / needle one-shots                     16 KB
0x28000-0x283FF   TAPE stream double buffer (2 x 224 B, padded)                    1 KB
0x28400-0x2C3FF   TAPE ring: raw 8-bit PCM shadow render / capture drain (data)  16 KB
0x2C400-0x3BFFF   SFX (today ~27 KB ADPCM; room to grow)                         63 KB
0x3C000-0x4BFFF   area bank (swapped per overlay group: organ, factory, soda...) 64 KB
0x4C000-0x67FBF   SCORE LIBRARY: every cue's tokenised phrases (data, DMA'd
                  into a 4 KB main-RAM window when a cue starts)                ~112 KB
0x67FC0-0x7FFFF   reverb work area (0x18040: fits Echo / Delay)                  96 KB
```

- **SPU RAM as tape and as a bookshelf.** The SPU can only *play* ADPCM, but
  its RAM is plain memory that the CPU reads back by DMA (the current audio
  self-test already does SPU → RAM read-back). So:
  - the **TAPE ring** (16 KB = 1.5 s at 11,025 Hz) lives in SPU RAM. The
    shadow renderer uploads one 184-byte block per frame, and during a
    scratch the CPU reads back a ~2 KB window around the playhead each frame;
  - the **score library** lives there too, and a cue's phrases are DMA'd
    into a 4 KB main-RAM window when the cue starts (a few KB, well under a
    millisecond).

### 8.6 PS1 budgets

| Resource | Cost |
|---|---|
| Main RAM | SPU driver code ~6 KB + core arranger code (shared with the GBA) ~10-14 KB + score window 4 KB + TAPE staging 2.5 KB + DECK log 1 KB + state 1 KB ≈ **25-29 KB resident** (the scene-specific cue *data* is in SPU RAM, not in overlays) |
| SPU RAM | the map above: everything fits, including the largest reverb |
| CPU | the sequencer and microtick IRQ ~1-2 %; the TAPE shadow render 6.5 % while armed (2D only); the TAPE encode 2 % during a scratch; DECK ~0.2 % |
| CD | area banks with the overlay read; nothing streamed during play |

---

## 9. Parity matrix: one composition, two native renderings

| Feature | GBA | PS1 |
|---|---|---|
| Pulse at fixed duties | PSG pulse 1/2 at 12.5/25/50/75 %; soft pulse | ADPCM 16-duty bank |
| **PWM** | soft PWM (true, per sample); PSG duty cycling | repeat-address morph; inverted detuned saw pair; baked PWM loops |
| Saw / sync saw | soft saw / sync saw; wave-channel saw | ADPCM saw; baked sync-saw loops |
| Triangle / wavetable bass | PSG wave channel (dual-bank morph, 64-step) | ADPCM tri, stepped tri, morph loops |
| One-channel power chord | wave-channel 64-step chord cycle | power-chord ADPCM cycle |
| Noise drums | PSG noise 15/7-bit with clock divider; soft PCM kits | noise ADPCM at per-voice pitch; ADPCM kits; NON for live noise |
| Pitch sweeps and slides | PSG pulse-1 hardware sweep; microtick slides | microtick pitch-register slides |
| Vibrato | software (microtick) | software, or PMON with a silent LFO voice |
| FM-ish timbres | soft 2-op FM-lite (tier S) | PMON pairs (live) + baked FM loops |
| Echo / space | soft echo (tier S) + echo-by-repeat | reverb presets + echo-by-repeat |
| Stereo | FIFO A/B packed-stereo panning; PSG L/R bits | per-voice L/R volume + hardware volume sweeps |
| Ducking | PSG master volume + soft bus gain | voice volumes / volume sweeps |
| Filters / muffle | soft one-pole; PSG timbre swap | muffled wave banks (switched by repeat address) |
| Voices (beyond SFX) | 4 PSG + 2-11 soft music voices, by tier | 16 music voices |
| Choir (25 singers) | lead babble + up to 3 section voices + PSG harmony | 8 choir voices (SATB × 2) + babble |
| Babble in tune | yes (core) | yes (core) |
| Scratch DECK | yes (all tiers) | yes |
| Scratch TAPE | soft-mix ring + ghost PSG (tier S) | shadow render → SPU RAM tape → ADPCM stream |
| Scratch the singer | TAPE ring of the babble voice (soft) | hardware capture of voice 1 |
| Vinyl crackle | soft generator | crackle loop + NON hiss |
| Reverb freeze | echo feedback freeze | reverb feedback freeze |
| Sample-accurate timing | 240 Hz microticks, sample-locked (timer 1 on timer 0) | 240 Hz root counter |
| Live DJ controls | D-pad + A/B/L/R | D-pad + buttons; **analog stick as the platter** (DualShock) |

---

## 10. Core architecture and rollout

### 10.1 Modules (core, no floats, no malloc)

| File | Job |
|---|---|
| `mus_arrange.c` | phrase-graph walk, per-pass variation, reharm pools, groove DNA and fills, stems, the dial morph, key and tempo journeys, the event queue |
| `mus_state.c` | the drivers (section 3): smoothing, hysteresis, dwell |
| `mus_quote.c` | the motif library and quote slots, infection, hot/cold, true names |
| `mus_comic.c` | the comic-timing bridge: grids, elastic tempo, mute/duck windows, pre-roll |
| `mus_scratch.c` | scratch patterns, the DECK scrub and its event log, TAPE / SAMPLE requests to the platform |
| `mus_bed.c` | the ambient emitters |
| `mus_data_*.c` | cue data: generated from MML sources by `tools/mmlc.py` into tokenised byte strings (about half the size of MML text) |
| `audio.c` (existing) | keeps `audio_music(SONG_*)`, stingers, SFX and babble; legacy songs become one-node cues |

### 10.2 API sketch (append-only)

```c
/* core: doomo/music.h */
void mus_cue(int cue, int grid);                    /* MC_*, MQ_NOW/16TH/BEAT/BAR/PHRASE */
void mus_param(int param, int value);               /* MP_DANGER, MP_HELL, MP_HOUR, ... */
void mus_sting(int sting, int grid);                /* STG_* */
void mus_scratch(int pattern, int grid, int source);/* SCR_*, SRC_DECK/TAPE/SAMPLE/AUTO */
int  mus_comic(int event, uint32_t land_tick, int grid, int hold_max); /* returns the land tick */
int  mus_ticks_to(int grid);                        /* microticks to the next grid point */
void mus_mute(uint32_t from, uint32_t to);          /* trailer-style mute window */
void mus_duck(uint32_t from, uint32_t to, int gain);
void mus_quote(int motif, int flags);               /* request a quote (priority/cooldown apply) */

/* platform.h additions: keep plat_tone() for 4-channel legacy */
typedef struct DoomoAudioCaps { uint8_t voices, tier, has_tape, has_reverb, has_pmon; } DoomoAudioCaps;
void plat_audio_caps(DoomoAudioCaps *out);
void plat_music_events(const DoomoMusEvent *ev, int n);  /* timestamped (microtick 0..7), role, wave id,
                                                            pitch (Hz Q8), vol L/R, slide, flags */
void plat_audio_fx(int fx, int a, int b);           /* echo, reverb preset/freeze, duck, crackle, arm tape */
void plat_scratch(const DoomoScratch *s);           /* start / steer a TAPE or SAMPLE scratch */
```

### 10.3 MML extensions (append-only; existing songs parse unchanged)

| Token | Meaning |
|---|---|
| `{A}` ... `{/}` | phrase (node) markers inside a cue's role string |
| `y<n>` | pan (0 = L .. 8 = C .. 16 = R) |
| `j` | glide (portamento) into the next note (Vex) |
| `D<a>,<b>,<rate>` | duty / PWM sweep |
| `W<n>` | wave id beyond `WAVE_*` (bank waves: morph, vowel, chord, FM) |
| `E<delay>,<vol>` | echo-by-repeat on the partner channel |
| `?<p>` | optional note (probability, for groove DNA and ornaments) |
| `%` | variation slot (where 5.2 ops may apply) |
| `M<motif>` | quote slot |
| `!` | accent (velocity table) |

### 10.4 Compatibility and tests

- The 19 songs keep working through `audio_music()`; `test_audio` keeps its
  per-song length checks. Each song becomes the A-node of its cue.
- The Studio's `DoomoSong` keeps its size; the scratch nibble (6.6) is
  append-only, and `song_sanitize` masks it.
- New native tests (5.13): event hashes per cue under driver scripts, graph
  properties, the repetition metric, stinger grid accuracy, DECK return
  accuracy. Platform harness tests: the GBA `profile` budget per tier
  (soft-mixer cycles under the tier caps), `expect-audio` windows for
  stereo FIFO B; the PS1 voice-count and SPU RAM map self-tests, and a TAPE
  stream round-trip.

### 10.5 Rollout

1. **Arranger core on today's 4 channels** (both consoles at once): phrase
   graphs, variation, reharm, generative drums, stems, the dial, quantised
   transitions, the comic bridge, DECK scratches. Even with 4 channels this
   delivers most of the "non-repetitive" brief.
2. **GBA mixer v2** (stereo, packed panning, lean kernels, tiers, governor,
   microtick clock) and **PS1 voice expansion** (16 music voices, banks,
   ADSR mapping, reverb per area, root-counter microticks).
3. **TAPE scratches** (GBA ring + ghost PSG in `gba_big`; PS1 shadow render
   into the SPU RAM tape + the ADPCM stream) and the crackle.
4. **Authoring the cue sheet** (section 4): `MC_*` cues from the existing
   songs plus new phrases, motifs and beds.
5. **Studio scratch lane, Concert DJ booth, Jukebox versions.**
6. **Rich kernels** (the IWRAM overlay window), PMON FM, repeat-address
   morphs, polish.

---

## 11. To reconcile with the story spec, and open questions

| Topic | What this spec assumes | Needs from story |
|---|---|---|
| **Comic-timing engine** | events `CE_*` (5.9) with a land frame, a grid and a `hold_max`; trailer-style mute/duck windows | its real event names and timing units; whether it can hold frames for the beat |
| **Episodes / deeper chapters** | an episode card, a "previously on" REWIND recap, a cliffhanger STOP | the episode list and their emotional arcs, so each card gets its reharmonisation |
| **80+ side missions** | families with layers and stings (4.6), not unique songs | each mission's family tag; which ones deserve a unique cue (e.g. a boss hunt) |
| **Dream House** | rooms as stems, `comfort` / `spook` / `fort` drivers, the variants in 4.17 | the room list, how comfort / spookiness / barricades are computed, siege beats, whether the GBA isometric renderer needs `gba_big` (tier M vs S) |
| **New characters** | motifs for the cast in the credits; a TRUE NAME generator for everyone else | any new major character who needs a hand-written motif |
| **Credits** | the march locked at 163.6 bpm (one pair per beat at `RANK_TICKS` = 11) | confirm, or choose a tempo and change `RANK_TICKS` |
| **Integration memory targets** | GBA resident +2.4 KB EWRAM, borrowed `gba_big`; flash routines on the stack; the IWRAM overlay window. PS1 ~25-29 KB resident, data in SPU RAM | sign-off from the integration and memory owners |

**Open questions (platform):**

- **GBA hardware checks:** whether to use 8-bit / 65.5 kHz `SOUNDBIAS` PWM
  resolution at the 21 kHz tier (less aliasing, but test on GBA, GBA SP and
  DS-mode hardware); IRQ nesting during the vblank mix.
- **PS1 hardware checks:** repeat-address switching with banks encoded
  without loop-start flags; silent PMON modulators; the reverb freeze's
  stability at high feedback.
- **Accessibility:** the "Gentle audio" option (no scratches or sudden
  stingers) and a music intensity cap.
