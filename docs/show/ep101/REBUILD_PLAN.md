# Ep101 "Mingle Night": rebuild plan in the pilot_proof style

How to rebuild the whole 22:00 pilot (`PILOT_SCRIPT_v2.md`) with the toolkit
in `pilot_proof/` (`wd` world camera, `mayor`, `sets`, `story`, the teaser
synth, `assemble.sh`). The cold open (1:42) is already built:
`show_out/proof/cold_open.mp4`.

## Ground rules (from the cold open)

- **Pacing.** Shots run 4-8 s; comic holds and the three protected holds can
  run longer. The energy lives inside the shot: squash and stretch, springs,
  camera push-ins and drifts, pop-in sound words, and small shakes on hits.
  Use fast cuts only where the script marks them (`[INSERT 4]`, `[TRUTH 4]`).
- **Shot count.** Plan one shot per 6 s of scene, rounded.
- **Look.** Native 240x160, scaled 6x and pillarboxed to 1080p. Everything is
  drawn in world space: no sprites stretched, and limbs drawn with
  `wd_tube`, never thin lines.
- **Words.** No readable word may form from two overlapping signs. Check every
  frame where text overlaps (see the MINGLE ghost rule).
- **Audio.** Locked takes go through the `assemble.sh` dialogue chain. Every
  spoken line without a take gets a dialogue box (or a bubble) with babble.

## Shared toolkit work (do once, first)

| # | Item | What it gives | Days |
|---|---|---|---|
| T1 | **Cast library** (`cast/`): one posable drawer per character on the `mayor.c` pattern (parts, expressions, `look`, `squash`, `fan`/ears, hands via `wd_tube`) | Every scene | 1 per lead, 0.5 per minor (see below) |
| T2 | **Limb and hand helpers**: arm/leg chains (shoulder, elbow, wrist) with reach-stretch, paw, wing and bony-hand ends, plus a two-bone IK `aim()` | Every acting shot | 1 |
| T3 | **Lip-sync helper**: read each take's WAV envelope (or Rhubarb visemes if available) and drive `mouth`/`beak` per frame by line ID | All dialogue scenes | 1.5 |
| T4 | **Speech-bubble option** beside the dialogue box: auto-placed tail and safe area, so bodies stay visible in medium shots | All dialogue scenes | 1 |
| T5 | **Line table driver**: place takes by line ID and frame from `lines.csv`, so no hand-typed delays in `assemble.sh`, and keep the `[R5]-[R26]` gaps | All dialogue scenes | 1 |
| T6 | **Shot DSL / timeline per act**: one file per scene, shots registered by name; renders by scene or by act; contact sheets per scene | Build speed | 1 |
| T7 | **Crowd system**: N citizens with bit-actions (carry, sweep, wave, cheer, flee) and Mascotty always still | Fair, stampede, bandstand | 1.5 |
| T8 | **Filters**: CUTE-skin to HELLIFY ramp (exists in the teaser as `tk_post_hellify`), the 21 f rot, and the child guard (no hellify on frames with a child) | Acts 2-4, MT | 0.5 |
| T9 | **First-person host**: port the teaser's s10 raycaster to the world camera at 16:9 with the HUD | SC42, SC44 | 2 |
| T10 | **Cards**: act cards, clipboard inserts, eyecatch out/in pairs, credits crawl | All acts | 1 |

Toolkit total: about 12 days, plus the cast.

## Cast to build (T1)

| Character | Notes | Days |
|---|---|---|
| Mayor Giggs | Done (turkey); add walk cycle, clipboard hand, mic | 0.5 |
| Sunny | Silent bunnykin lead: all acting is ears, eyes and posture; suitcase | 1.5 |
| Nana Puddin | Sheepkin, woolly; the paw already exists | 1 |
| Pip + Mr. Flops | Tiny bunnykin; child rules: cute filter always, no hellify in frame | 1 |
| Bonejangles | Skeleton, playable ribs, possessed variant with 8 f Clinker lag | 1.5 |
| Father Fuzz | Plushie bear priest | 1 |
| The Pet | Egg, crack stages, hatch, beep | 0.5 |
| Dr. Bloom, Steamy, Kevin (three eyes), Hattie, Miss Lulla | Minor; share one plushie/critter rig with parts swaps | 0.5 each |
| Puff demons, the Big Puff, Clinker's breath puff | Fluff and fang; burst to mist and gibs on demons only | 1 |
| Mascotty | Done (still); reuse | 0 |

Cast total: about 11 days.

## Build order (highest impact first)

1. **SC21 Bandstand welcome (1-08)**: the Mayor's showcase, 45 s, the
   tightest comic chain. It proves T3-T5 on the most lines.
2. **SC22 Bake tent (1-09)**: Nana and the egg; the heart of Act One.
3. **SC46 + SC56 act-outs (3-07, 4-08)**: the emotional peaks and protected
   holds; trailer-able.
4. **SC31-SC33 possession and stampede (2-02 to 2-04)**: the act-break spectacle
   and the hatch; proves T7 and T8.
5. **SC42 + SC44 first person**: reuse the teaser raycaster; high wow per day.
6. The rest of Act One (the Giggle crawl SC17-SC20 share one fair set).
7. Act Two remainder, Act Three remainder, Act Four remainder.
8. Tag, credits, post-credits, eyecatches, act cards (cheap, last).

## Scene plan

Runtime from the script's timecode lines. Shots = 4-8 s each. Effort is in
build days after the toolkit and cast exist: about 0.5 day per 10 s for scenes
on existing sets, 0.8 day per 10 s with a new set.

### Main title (SC07-SC13, 0:50)
Locked block from the theme work. Reuse the teaser's title pipeline and the
final theme master; add the cast pop-ins once T1 exists. **New art:** logo drop, gate with seven padlocks.
**Records:** none. **Effort:** 2 days.

### Act One "18:00 Welcome!" (5:20)

| Scene | Runtime | Shots (energy inside) | New art | Reuse | Record (creator) | Days |
|---|---|---|---|---|---|---|
| SC14 1-01 act card, Candy Hills road | 0:18 | 3: clipboard lines pop every 8 f with a tick; Sunny walks in with the suitcase, ears bounce | Road at sunset, clipboard card | Sky, hills | none | 1 |
| SC15 1-02 town sign | 0:17 | 3: sign creaks, Sunny reads, ears swivel | Town sign | Plaza buildings | none | 0.8 |
| SC16 1-03 fountain, park gate | 0:13 | 2: the fountain giggles 8 f late; Sunny's double take | Park gate | Fountain (done) | none | 0.6 |
| SC17-SC20 Giggle crawl: hook-a-duck, face painting, Hattie's tent, craft tent | 0:52 | 9: one long crawl with camera drifts, each stall a gag beat | Fair stalls (one kit), duck pond | Crowd (T7), lanterns | Steamy, Kevin, Hattie, Lulla lines | 4 |
| SC21 1-08 bandstand welcome | 0:45 | 7: spotlight, patter, page, a 4 s creep, freeze-deadpan-hold, fountain, fanfare | Mic, spotlight, pages | Bandstand, Mayor | **All 11 Mayor lines** | 3 |
| SC22 1-09 bake tent | 0:55 | 9: clipboard insert, bite test (15 f bites), pie lift, egg hand-over | Bake tent interior, pies | Bakery props, egg | **Nana 13 lines**, Mayor bites | 4 |
| SC23 1-10 lantern string | 0:28 | 5: Pip's patrol, lantern gag | Lantern rig | Fair kit | Pip, Mayor | 1.5 |
| SC24 1-11 edge of the crowd | 0:18 | 3: walla, Sunny alone | none | Crowd | walla (TTS) | 0.8 |
| SC25 1-12 bandstand 20:00 | 0:20 | 4: pages squared like a newsreader, THUMP THUMP | none | Bandstand | Mayor | 1 |
| SC26 1-13 Giggle 21:00 | 0:24 | 4: the crowd turns, held beat | none | Fair, crowd | 2 lines | 1 |
| SC27 1-14 the boards, act out | 0:30 | 5: Clinker's voice under the boards | Boards close-up | Bandstand | Clinker (in sync) | 1.5 |

### Act Two "21:00 (we don't do that)" (4:15)

| Scene | Runtime | Shots | New art | Reuse | Record | Days |
|---|---|---|---|---|---|---|
| SC30 2-01 act card | 0:22 | 4 | Card | Bandstand | 20 words | 1 |
| SC31 2-02 possession | 0:33 | 6: ribs breathe, Clinker lags 8 f | Possessed Bonejangles | Bandstand | **Bonejangles + Clinker pairs** | 2 |
| SC32 2-03 fun fair | 0:30 | 5 | none | Fair kit | 11 lines | 1.5 |
| SC33 2-04 stampede + hatch | 0:35 | 6: crowd flood, close on the egg, tock every 6 f, hatch | Hatchling pet | Crowd, egg | beep x1 (raw piezo) | 2.5 |
| SC34 2-05 bandstand steps | 0:28 | 5 | Steps | Bandstand | Fuzz, Bloom | 1.5 |
| SC35 2-06 bandstand | 0:40 | 7 incl. a sung phrase | none | Bandstand | Mayor, Fuzz | 2 |
| SC36 2-07 the boards | 0:40 | 7 | none | Boards | 8 lines | 2 |
| SC37 2-08 whack-an-imp | 0:27 | 5: silent gag chain, beep | Whack-an-imp booth | Fair kit | beep | 1.5 |

### Act Three "22:00 Bedtime! weather" (4:15)

| Scene | Runtime | Shots | New art | Reuse | Record | Days |
|---|---|---|---|---|---|---|
| SC40 3-01 act card, fun fair (cute mode) | 0:50 | 8, elastic ±20 s | Puff demons | Fair kit | 6 words | 2.5 |
| SC41 3-02 bake tent | 0:15 | 3 | none | Bake tent | beep | 0.6 |
| SC42 3-03 not-scary house (first person) | 0:40 | 1 continuous first-person run with cuts on hits | House interior tiles | T9 raycaster, HUD | none | 2 |
| SC43 3-04 bandstand, **protected hold 1** | 0:40 | 4 long holds | none | Bandstand | 22 words | 1.5 |
| SC44 3-05 bake tent (first person, hellified) | 0:30 | first-person run | Hell bake-tent tiles | T9, T8 | none | 1.5 |
| SC45 3-06 steps (silent) | 0:35 | 6 | none | Steps | none | 1.2 |
| SC46 3-07 act out | 0:45 | 8: Clinker's breath puff, chalk dust | Breath puff | Bandstand | **Clinker, Bonejangles, 18 words** | 2.5 |

### Act Four "23:00 ________" (3:55)

| Scene | Runtime | Shots | New art | Reuse | Record | Days |
|---|---|---|---|---|---|---|
| SC49 4-01 act card | 0:20 | 3 | Card | Bandstand | 1 line | 0.8 |
| SC50 4-02 bandstand | 0:30 | 5 | none | Bandstand | 21 words | 1.2 |
| SC51 4-03 sealed gate, **protected hold 2** | 0:45 | 5 long holds | Gate close set | Wall | 36 words | 2 |
| SC52 4-04 steps | 0:20 | 4 | none | Steps | 7 words | 0.8 |
| SC53 4-05 bandstand | 0:25 | 4 | none | Bandstand | 22 words | 1 |
| SC54 4-06 the Mingle crate | 0:35 | 6 | Crate, rocket | Bandstand | 22 words | 1.5 |
| SC55 4-07 joke graveyard path | 0:12 | 2 | Graveyard path | Hill | none | 0.8 |
| SC56 4-08 Launch Hill, **protected hold 3** | 0:48 | 6 incl. rocket launch over the Wall, fireworks | Launch stand, park panorama | Fireworks (story.c), Wall | Mayor, Nana, Pip | 3 |

### Eyecatches, tag, credits, button

| Scene | Runtime | Notes | Days |
|---|---|---|---|
| SC28-29, SC38-39, SC47-48 eyecatch pairs | 3 x 0:10 | Out/in cards with one change each (T10) | 1.5 |
| SC57-58 tag, Pink Door cottage | 0:35 | Interior set; 12 words (Nana) | 2 |
| SC59 credits | 0:30 | Crawl over stills | 0.5 |
| SC60 post-credits gate | 0:10 | Grille, the purr, Gurgle's captioned words | 0.8 |

## Recording needs (creator)

Priority is by on-screen weight; everything else can stay TTS or babble.

1. **Mayor Giggs**: all 35 lines, plus re-records of "...Hee. Hee.", "Perfect."
   and "Stay." to replace the library and TTS takes in the cold open.
2. **Nana**: 22 lines (SC22 carries the act).
3. **Bonejangles + Clinker**: 15 + 12, recorded as same-word pairs (Clinker in
   sync; the engine slides her 8 f late).
4. **Pip**: 16 lines; **Father Fuzz**: 16 lines.
5. Minor roles: Steamy 5, Bloom 5, Kevin 3, Hattie 3, Lulla 1, Gurgle 3 plus the purr.
6. Not performed: walla (22 lines, TTS), the pet's beeps (raw piezo), and Sunny (silent).

Each take is named by line ID (`lines.csv`) so the T5 driver places it.

## Schedule (budget weeks of 5 build days)

| Week | Work | Output |
|---|---|---|
| 1 | T2, T3, T4, T5, T6; Sunny, Nana | Toolkit, two leads |
| 2 | T7, T8, T10; Pip, Bonejangles, Fuzz, Pet; fair kit | Cast core, crowd |
| 3 | SC21, SC22, SC14-SC16 | Act One spine (about 2:30) |
| 4 | SC17-SC20, SC23-SC27 | **Act One complete (5:20)** |
| 5 | T9; SC30-SC33; minor cast | Act Two first half, the hatch |
| 6 | SC34-SC37; SC40-SC42 | **Act Two complete**; Act Three start |
| 7 | SC43-SC46; SC49-SC52 | **Act Three complete** |
| 8 | SC53-SC56; tag, eyecatches, credits, button, main title cast pop-ins | **Act Four + wrap** |
| 9 | Conform to 22:00, the final mix, a QA pass on every frame (word-overlap and child-filter checks), and fixes | **Locked 22:00 pilot** |

Totals: about 12 toolkit days + 11 cast days + 75 scene days, which is too
much for 9 weeks of one lane. Run two lanes from week 3: lane A builds scenes,
lane B builds cast and sets one week ahead. Without a second lane, plan 14
weeks.

**Risks:**
- Recordings arriving late: build with TTS and swap takes by line ID.
- First-person scenes need the raycaster port; fall back to the trailer's
  4:3 build if T9 slips.
- Crowd scenes cost the most render time; keep crowds under 20 actors.
