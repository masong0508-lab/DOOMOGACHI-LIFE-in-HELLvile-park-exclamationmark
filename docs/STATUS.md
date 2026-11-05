# Status: what is playable, what is left (2026-11-05)

A read of the repo plus one native run: `make -C core test` on this tree finished with 0 failures
(154 scripted playthroughs, 19 press journeys at 240x160 and 320x240), and the native runner links
69 scenes, none of them a weak placeholder. **Nothing here was built or run for GBA, PS1 or
Dreamcast** (no ARM/MIPS/SH4 toolchains, no emulator). Console statements below come from the docs.

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

## Playable now (core, native runner)

| Area | What works |
|---|---|
| Title + Free Play | title, New Island (empty / 1000-citizen Classic), island hub, apartments, roster, care, shop, news, options, save/load |
| Creator | species, body, presets, age and Hellvile looks |
| Life sim | needs, social, rent, Doomogachi pet + Doomodex, exorcism, dreams, family dashboard + add-member wizard, births, ageing, pets, loss + memorial, New life, 16 household events, New Game+ |
| Shooter | Hell Breach levels (first / third person), arena modes, welding workshop |
| Story | chapters 1-4 + epilogue, cutscenes, quest log, about 109 live side quests, ending engine with routes and secret endings |
| House | Live / Build / Photo, HomeMart, Black Market, Hold the Door |
| Extras | 7 Fun Fair minigames, Concert Hall, Music Studio, Library, fishing, cooking, 8 languages (Spanish furthest) |

## Left to do

1. **Verify on the consoles (biggest gap).** Much recent GBA/PS1 work is "written without a compiler" or "not run on
   PCSX-Redux": the PS1 harness re-run (ROADMAP Tiny 2), the U1p config file, J6/J7 on the disc, the GBA press latch and
   hub B shortcut (`docs/HARDWARE_CHECK.md`), family UI on console screens.
2. **Dreamcast.** Skeleton only (draws nothing). Needs DC3-DC9 and the Flycast approval.
3. **GBA speed.** Island hub ~7.5 fps, town hall 6.5, combat 10-12, pet 10, concert 13, title 12-14 (G3, G8-G12).
4. **Features not built.** Mature pack art (loaders exist; PS1 has no stills), multiplayer + profiles (MP0/MP1), deep
   customization (P0-P6), console music backends + TAPE scratches + remaining cues, Ninth Cloud map and Breach/Arena
   story variants, Feast scene (S186), baby outfits and elder stoop.
5. **Script and languages.** W9-W12, W15 (approved, unwritten), W16; re-translate rewritten text (L8); pt-BR, fr, de, it,
   nl, pl not translated; Chinese and Japanese need a new font path.
6. **Housekeeping.** The saved branches (G3, G9, G11, H7, L4, W6, W7, ...) are not in the repo. README "Status" still says
   the game build is a developer launcher with placeholders; the code no longer matches.

Sources: ROADMAP.md, docs/PS1_LIFESIM.md, docs/HARDWARE_CHECK.md, docs/NEXT_SESSION_PROMPT.md, show/DEFERRED.md.
