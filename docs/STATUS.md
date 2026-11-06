# Status: what is done, what is playable, what is left (2026-11-06 refresh)

A read of the repo plus native runs: `make -C core test` on this tree finished with 0 failures (861 tests;
160 scripted playthroughs, 19 press journeys at 240x160 and 320x240), `make -C core story-check` and `check-gen` clean, and the native runner links
69 scenes, none of them a weak placeholder. **PS1 and Dreamcast were not built or run
here** (no MIPS/SH4 toolchains, no emulator); their statements below come from the docs. **GBA was** (2026-11-06 evening update: a sandbox with apt built the
ROM and ran mGBA): the invaded map measures 14.9 drawn fps / 29.8 logic ticks/s, work avg 56.8 ms, max 76.5 ms, sprites ~40% of the frame
(`docs/INVADED_SPEED.md`); the next steps are exact sprite-table and setup savings, then the spike frames.

The same lists, with the order to take the open items and what needs the owner, are in `ROADMAP.md` "Refresh 2026-11-06" (that section is the one kept current).

## Customization P1.M2 (portrait deep fields), 2026-11-05

- **Built:** 13 wave-2 face/head painters + 6 halo styles (`DF_PUPIL`, `DF_SCLERA`, `DF_BLUSH2`, `DF_GLASSES2`, `DF_EAR_FOLD`, `DF_HOOD_HEAD`,
  `DF_LASH`, `DF_LID`, `DF_LINER`, `DF_BAGS`, `DF_FRECKLE`, `DF_MOLE`, `DF_SCAR2`), unit suite 828/828.
- **Playable:** none of it yet. Only tests set deep values; no creator row, save field or Look Book entry reaches them, so
  portraits in play are unchanged.
- **List appends:** B76 halos +6 painted (appended after the live count, not selectable yet). 12 lists to go: F01, F07,
  F25, F29, F35, F47, H01, H13, E01, B66, B79, O25.
- **Left:** the other 12 list appends, flipping lists live (needs P6 premades), art review and contact
  sheets, then M3 (`DF_MAKEUP`, `DF_FACEPAINT`, `DF_AURA2`, A28 expressions, aura/halo), P0's real deep-field set, `live_p1.txt`,
  the mGBA bench and P2-P6. Details: `docs/PORTRAIT.md`, `ROADMAP.md` "Customization P1.M2 status".

## Done (on the branch, core-tested)

- **Life sim core:** hour-by-hour fast-forward, an edge pass that never skips an edge, best-friend / nemesis hysteresis (lifesim core 2, `core/tests/test_sim_core.c`; one 9 h skip costs about 7x the old single pass on the host, once per night's sleep, so re-measure it on the GBA); family table and pool members (F1), family screen, add-member wizard, care-all (F2), house occupancy (F3), ageing, births and
  family events (F4), pets with needs, old age and the uplift (F5, finished by patch `doomo-lifesim1-pet-uplift-nov05i`), loss, memorials and
  flowers (F6), free will, bonds and moods (F7/F8), helpers and away trips (F9), disasters and story danger hooks (F10), life events (LS3/LS4),
  household tiers and New Game+ (S-HOUSE), play-as and New life (PA1-PA9), profile guests (MP0 core), endings and romance routes (E1-E4, E12).
- **Build and content:** settings core and options redesign (U0, U2), Continue-first title and quick saves (U6, U3), first-boot language picker and
  Mature gate (U5, E11), content-pack format and readers (E2, E7), mature disc/ROM adapters for GBA, PS1, Dreamcast (E8-E10, host-tested only),
  script chapters 1-4 + epilogue and side quests (W1-W14), 8 language engine, house (H-series), GBA speed work (G1-G7).
- **Customization:** gene codec, 13 wave-2 face/head painters and 6 halos (P0.M0a, P1.M2a-e), none selectable yet.

## Playable now (core, native runner)

| Area | What works |
|---|---|
| Title + Free Play | title, New Island (empty / 1000-citizen Classic), island hub, apartments, roster, care, shop, news, options, save/load |
| Creator | species, body, presets, age and Hellvile looks |
| Life sim | needs, social, rent, Doomogachi pet + Doomodex, exorcism, dreams, family dashboard + add-member wizard, births, ageing, pets (they grow old, and after the story you can uplift a close pet into a talking, playable housemate), loss + memorial, New life, 16 household events, New Game+ |
| Shooter | Hell Breach levels (first / third person), arena modes, welding workshop |
| Story | chapters 1-4 + epilogue, cutscenes, quest log, about 109 live side quests, ending engine with routes and secret endings |
| House | Live / Build / Photo, HomeMart, Black Market, Hold the Door |
| Extras | 7 Fun Fair minigames, Concert Hall, Music Studio, Library, fishing, cooking, 8 languages (Spanish furthest) |

## Left to do

**Life sim core (the open parts, in order)**
1. (Part 2, per-avatar pockets, is done: `docs/PLAYAS.md` section 9.)
2. Part 3: New life from the title screen and in a fresh world (today it is the character switcher, on a finished island only).
3. Part 4 (picker scene done 2026-11-05): typed guest names, renaming, the Free mode gate and the co-op hero characters done; still open: co-op hero content and local multiplayer (MP1), the story house unlock decision (S-HOUSE needs a definition).
4. Smaller: pets' own look when uplifted, baby outfits, a baby body shape, elder stoop, a graveyard scene for the household (the Remembered page serves), the hand-written disaster scripts (need your tone check).

**Everything else**
1. **Verify on the consoles (biggest gap).** Much recent GBA/PS1 work is "written without a compiler" or "not run on PCSX-Redux": the PS1 harness re-run,
   the U1p config file, J6/J7 on the disc, the GBA press latch and hub B shortcut (`docs/HARDWARE_CHECK.md`), family UI on console screens.
2. **Dreamcast.** Skeleton only (draws nothing). Needs DC3-DC9 and the Flycast approval.
3. **GBA speed.** Island hub ~7.5 fps, town hall 6.5, combat 10-12, pet 10, concert 13, title 12-14 (G3, G8-G12).
4. **Features not built.** Mature pack art (loaders exist; PS1 has no stills), multiplayer (MP1), deep customization (P0-P6, M2 list appends, M3),
   console music backends + TAPE scratches, Ninth Cloud map and Breach/Arena story variants, Feast scene (S186).
5. **Script and languages.** W9-W12, W15 (approved, unwritten), W16; re-translate rewritten text (L8); pt-BR, fr, de, it, nl, pl untranslated;
   Chinese and Japanese need a new font path.
6. **Housekeeping.** The saved branches (G3, G9, G11, H7, L4, W6, W7, ...) are not in the repo. README "Status" still says the game build is a developer launcher.

Sources: ROADMAP.md, docs/PS1_LIFESIM.md, docs/HARDWARE_CHECK.md, docs/NEXT_SESSION_PROMPT.md, show/DEFERRED.md.
