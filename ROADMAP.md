# DOOMOGACHI roadmap (what's left)

Last updated 2026-11-01 (evening: F4 parts 1-2, F6 core, life stage forms, E6a/E6b start). The decisions behind this list (tone, PG-21 register,
show canon, life-sim answers, 32 MB GBA cartridge) are summarised at the end.
Package IDs match the planning files; each package is built in its own worktree,
reviewed, then landed on `claude/nice-allen-gr6czm`.

## Current focus (standing instruction, set 2026-11-02)

**Focus on the PS1 build from now on, until the owner says otherwise.** Pick PS1 work first (disc build, harness, `docs/PS1.md`
budgets, PS1-side timing and memory) and only take GBA, Dreamcast, show or script work when the owner asks for it by name.
Small shared-core patches that already fit the patch pattern (like U3b) are fine when asked for.
The PS1 life-sim status checklist (what is verified, written, or missing on that build) is `docs/PS1_LIFESIM.md`.

## 0. Status at a glance

**Done and on the branch**
- U6a (Continue-first title): with a story save the title's first row is Continue; boot -> story is 3 presses (J2).
  U6b (2026-11-02): Free Play with no island saved goes straight to New Island, and the "name your island?" prompt
  starts on "Maybe later": J9 13 -> 11 presses, New Island -> hub 8 -> 7. Budgets lowered (j9, life_new_island, j10, j11).
  U6e (2026-11-02): the island welcome's controls hint is a toast, not a second dialogue box: J9 11 -> 9 presses,
  New Island -> hub 7 -> 5. `creator_hellvile` passes (checked on the tree with PA4 + U6c); no U6d patch was needed.
  U3c (2026-11-02): START pressed inside the island's START menu quick-saves slot 1 and closes the menu (no row moved):
  J7 7 -> 2 presses, budget 3. `make -C core test` 677/677 on this tree (also confirms F4 parts 1-2, F6 and the life stage forms build and pass).
  U3b (2026-11-02): START pressed inside the story overworld's pause menu quick-saves and closes it (no row moved; strolls
  still aren't saved, there START just closes): J6 9 -> 2 presses, budget 3. `make -C core test` 677/677 on this tree.
  PA4: plot content now offers itself only to the hero (auto story beats, plot talk, side spots, nest fights).
- Script: style guide + locks (P0), Chapters 1-4 + epilogue rewritten (W1-W4),
  the 49 old quests rewritten (W5).
- House: golden coverage (H1), themed doors/windows + Towers 7B / Hell Pits rooms (H4),
  garden/plot (H5), animations (H6), lighting (H8), back facings (H9), 57 furniture
  pieces redrawn + roofs, GBA EWRAM for HOME (H10). PS1 disc links again (H0).
- GBA speed: speed tour (G1), Mode 4 quick wins (G2), IWRAM overlay (G4),
  tile-mode overworld (G5), billboards as sprites (G6), retained HUD panels +
  "3D: Faster/Better" (G7). Cartridge is now a full 256 Mbit / 32 MB.
- Languages: N-language engine (L1), one text path everywhere (L2a), language docs (L11: `docs/LANGUAGES.md`).
- Dreamcast: SH4 cross-check (DC0a), KOS toolchain script (DC0b),
  headless harness (DC1, not yet run on real Flycast), dc/ skeleton (DC2).
- Dreamcast rumble (X2): `plat_rumble()` hook, called by every camera shake with its strength and length
  (Screen shake Off = no rumble), Puru Puru driver in `dc/src/rumble.c`. A separate
  rumble on/off row waits for U0 (the option table is full at 64).
- Script W14: the Fun Fair medal/feat hints (29 lines in `unlocks_minigames.c`) and the shop toasts
  (`fp_shop.c`) punched up in the game's voice; names, ids and thresholds unchanged. Spanish and the
  other languages need a re-translation pass (L8).
- Script W13: library flavour punched up (wing blurbs, Quillby's quips, 4 jukebox notes, 12 library trophy hints);
  same counts and order, English only until the L8 re-translation.
- Dreamcast boot easter egg (X8): hold A+B+X+Y+START at power-on for a one-second rainbow (`dc/src/egg.c`).
- PS1 config file (U1p): `bu00:BASLUS-DOOMOC` behind `plat_config_read/write`, `PLATF_CONFIG`
  (needs a PS1 harness run to confirm on PCSX-Redux).
- Life sim F1 part 1 (family table): `family.h` / `family.c`, household cap by tier, roles with the
  partner's LS0 gate, the LIFE chunk's family section; no UI yet (F2), babies and ageing are F4 (below).
- Life sim F1 part 2 (pool members): a 20 B identity each (look seed, name, pronoun), `family_resident()`
  builds the full resident on demand, the LIFE chunk's POOL section; pets are drawn by F5.
- House H11: the GBA keeps and saves the HOME state (`HOME_PERSIST` is on for every build; `-DDOOMO_HOME_OFF_GBA=1` is
  the way back). GBA EWRAM free 13,552 -> 10,120 B (+3,432 B; gate 12,352 B before, 8,192 B after), ROM +5.6 KB, build clean.
- Household tiers (S-HOUSE, tiers part): `family_cap()` now has its grants. Finishing the story gives tier 1
  (cap 3 -> 10, GBA 6) and finishing it again gives tier 2 (-> 20, GBA 10); `family_tier_story_done()` runs where the
  credits finish (`story_ch34.c`), shows one toast and keeps K0's rule of no table entry (`unlock_grant_quiet()`).
  `family_tier_sync()` gives tier 1 to a story save that was finished before the tiers. Tier 2 only becomes reachable
  once NG+ can actually start a second run (nothing does yet: `MODE_NEW_GAME_PLUS` is unlocked, never read).
- Life sim F4 part 1 (life cycle): pool members grow BABY 4 / TODDLER 6 / CHILD 10 / TEEN 10 town days into ADULT (`family_age_daily()` from
  `day_rollover`, progress in the low 5 bits of `FamilyMember.flags`), residents mirror the sim's stage, pets and adults do not age.
- Life sim F4 part 2 (births + toast): one "grew up!" toast per member that changes stage; `family_birth()` makes a BABY pool member
  with the CHILD role (needs the player's partner and room under the cap; `FAM_ERR_NOPARTNER`). Still to do in F4: family events,
  the birth/growing-up UI (F2's wizard), old age (F6). F4 parts 1 and 2, F6 and the life stage forms were written without compiling or testing: build and run
  `make -C core test` before relying on them.
- Life sim F6 (loss), core part: pool members can be lost: neglect (`family_care()` resets the count, a warning at 4 days, lost at 7),
  old age (a pool ELDER passes after 31 days in the household), `family_lose(id, cause)`, a memorial of the newest 18 losses
  (`family_memorial*()`), the LIFE chunk's LOSS section, `family_loss_set()` for the future "Loss" option (U0). A child is never lost
  (R5): minors only ever "move away". Still to do: the graveyard / memorial screen, the care-all UI (F2) that calls
  `family_care()`, `life_ending_facts().neglect_deaths` (LS5), a resident member's death in the sim, pets' own needs (F5).
- Life stage forms (F4): `look_for_stage()` (`core/src/portrait_age.c`): baby to elder body and face proportions on every portrait,
  body sprite and fighter image (docs/PORTRAIT.md "Life stages"). Still to do: baby outfits, a baby body shape, elder stoop and
  wrinkles, and a look on real hardware.
- Script E6a/E6b (started): PG-21 adult beats in Ch1-2 added so far: Hattie's "private shows" bit (`ch1/ep14.dsc`) and Vex's
  invoice line (`ch2/ep21.dsc`). More adult-only beats for Ch1-2 remain; run `make -C core story-check` after adding them.
- Life sim wave 1: shared registry/contracts incl. 15 endings, security code,
  family, pockets and profiles (K0), life stages + two-way adult-only romance
  gate (LS0), journey/tap meter (M0), PG-21 register + lint (E5).

**Started, saved on branches, not landed** (resume these first)
- Life sim wave 2: LS1, U0, E0, PA0, E6a, E6b (may land before the session ends).
- G3, G9, H2, W6 (branches fut-g3 / fut-g9 / fut-h2 / fut-w6).
- G11, L4, H7, W7 (branches fut-g11 / fut-l4 / fut-h7 / fut-w7).

**Blocked on the user**
- Approve building the Flycast emulator core (`make -C dc/harness flycast`);
  until then the Dreamcast build is never actually booted in tests.
- W15 (Bonejangles origin thread in the game) needs a yes/no.

## By effort: smallest first

Sizes: **S** = small (hours), **M** = medium (about a day), **L** = large (a few days),
**XL** = extra large (a week or more). Within each size, do the top lines first;
a package can only start once the ones it depends on (see the sections below) have landed.

### Tiny: finish what's already built (saved on branches)
1. U0 settings core (branch `fut-u0`): fix one GBA fps test failure, then land.
2. Re-run the PS1 harness after LS1.
3. Approve and build the Flycast core (`make -C dc/harness flycast`), boot DC2.
4. Resume and land the saved branches: G3, G9, G11, H7, L4, W6, W7.

### S: small
- Life sim: PA0 follow-ups, U1g (GBA config sector),
  E11 (mature filter + 18+ confirm), LS-IN (press latch), PA9 (romance/mature
  gates for heroes), PA3b (Spanish
  gender fixes), PA8 (house/activities follow the avatar), E10 (Dreamcast pack
  adapter), E12 (endings integration), Z1 (joint gate).
- Script: W15 (Bonejangles origin; needs your yes/no).
- Languages: L9 (language picker).
- Dreamcast extras: X0 (contracts), X3 (VGA/display options).

### M: medium
- Life sim: E7 (content-pack format), U3a (pause shell), U5 (first-boot language
  picker), PA3a (gender-aware text), PA5 (character switcher), PA2 (hero choice:
  SUNNY / MARVIN / your own), U6 (Continue-first title and flow), E6c (Ch3
  punch-up), LS-QA (fewer taps), E6d (Ch4 + epilogue punch-up), LS6 (streamlined
  progression), E8 (GBA mature ROM), E9 (PS1 mature disc), LS5 (story/combat ties),
  PA6 (new life as a villager), PA7 (play-as content), U4 (held-frame backdrop),
  F4 (family events), F3 (house occupancy), PA10 (play-as integration),
  F5 (pets + uplift), F6 (loss/death), FREE (free mode), MP0 (profiles).
- GBA speed: G10 (pet, concert, title, dialogue), G12 (60 Hz, optional).
- Script: W8 (side_a part 3), W12 (free-play chatter), W16 (integration).
- Languages: L8a, L8b, L8c (Spanish batches), L8f (Spanish polish).
- Dreamcast: DC4 (input + VMU saves), DC7 (overworld), DC9 (full game + CI);
  extras X1 (VMU live pet), X4 (sharp 640x480 text), X5 (keyboard + mouse), X10 (VMU card trading).

### L: large
- Life sim: PA1 (play-as core), U2 (options screen redesign), E2 (mature scene
  slots), F1 (family core), E1 (ending engine), LS3 (life event engine),
  E3 (good + secret endings), E4 (bad + sexy endings, romance routes),
  PA4 (overworld handoff), LS4 (event and family content), F2 (family UI),
  U8 (GBA 3D Faster/Better done properly), S-HOUSE (story house unlock + tiers), MP1 (multiplayer + co-op heroes).
- GBA speed: G8 (tile backend: island hub and menus at 30 fps).
- House: H3 (PS1 room shells).
- Script: W9, W10, W11 (side_b rewrites).
- Languages: L2b (formatted text), L3 (tooling), L5 (story text for N languages),
  L7 (grammar-aware generators), L8d, L8e (big Spanish batches).
- Dreamcast: DC3 (PVR 2D), DC5 (AICA audio), DC8 (house renderer); extra X7 (DC-only area + costumes).

### XL: extra large
- Languages: L6 (PS1 language tables on disc), L10-pt, L10-fr, L10-de, L10-it
  (+ Dutch and Polish), then Chinese and Japanese (new font path).
- Dreamcast: DC6 (PVR 3D); extras X6 (VMU pocket pet), X9 (4-player party).

## 1. Priority 1: make the life sim playable on every build

Goal: Tomodachi-style life sim that is responsive, replayable and part of the
main game, working on native, GBA, PS1 and (later) Dreamcast.

| Wave | Packages | What it delivers |
|---|---|---|
| W2 (finish) | LS1, U0, E0, PA0, E6a, E6b | news as records + LIFE save chunk; settings core (OPT2); ending/pack contracts + VM ops; play-as contract; PG-21 punch-up of Ch1-2 |
| W3 | PA1, U1g, U1p, U2, E2, E7 | play-as core (PLAY chunk, story barrier, actor retargeting); GBA config sector; PS1 config file; **options screen redesign**; mature scene slots + host; content-pack format + `mpack.py` + templates |
| W4 | F1, E1, U3a, U5, PA3a, PA5 | **family core** (household up to 20 / GBA 10, Sims-style chosen relationships); ending engine; **pause shell**; first-boot language picker; gender-aware text; character switcher |
| W5 | LS3, PA2, U6, E11, E3, E6c | life event engine; **hero choice: SUNNY (female), MARVIN (male fox), make-your-own**; Continue-first title + flow; mature filter + 18+ confirm; good/secret endings; Ch3 punch-up |
| W6 | LS-QA, LS-IN, PA9, U3b, E4, E6d | fewer taps; press latch; romance/mature gates for heroes; story pause; bad + sexy endings and romance routes; Ch4 + epilogue punch-up |
| W7 | PA4, U3c, LS4, LS6, E8, E9 | overworld handoff; island START menu; event + family content; streamlined progression; GBA mature ROM variant; PS1 mature disc |
| W8 | F2, LS5, PA6, PA7, PA8, U4 | family dashboard + add-member wizard + care-all; story/combat/endings ties; **post-game "new life" as a villager**; play-as content; house/activities follow the avatar; held-frame backdrop |
| W9 | F4, PA3b, U8, E10 | family events + life cycle; Spanish gender fixes; GBA 3D Faster/Better done properly; Dreamcast pack adapter |
| W10 | F3, PA10, E12 | house occupancy; play-as integration; endings + mature integration |
| W11 | Z1 | joint gate across all four areas |

Added from the user's 30 answers (fold into the waves above):
- **F5 pets + uplift**: real (non-talking) pets that count toward the cap; late-game uplift.
- **F6 loss / death**: members can be lost, incl. starvation/deprivation.
- **S-HOUSE**: story unlocks the house; cap 3 -> 10 after completion -> 20 after NG+ (the tier grants are done, see
  section 0; the NG+ restart that reaches tier 2 and the story house unlock are not);
  deeper house build-up with better fetch quests.
- **FREE**: improved free mode; play-as anyone after completion; gated features.
- **MP0 / MP1 profiles + multiplayer**: per-player profiles inside one save
  (Animal Crossing flow), local multiplayer, helping in someone else's world,
  extra co-op heroes for the other profiles.
- Endings: 14 (good / bad / secret / sexy) + a 15th very secret "Nope." ending.
- First boot asks about mature content and sets an adults' security code.

Definition of done: each build (native, GBA, PS1) boots into the life sim, a
scripted run creates a family, plays as a villager, triggers an ending, and
saves/loads; all tests and speed gates pass.

## 2. Priority 2: GBA speed

Current GBA fps: maps ~29-30, story dialogue 25, invaded 15, combat 10-12,
island hub 7.5, town hall 6.5, pet 10, concert 13.

1. Land G3 (island hub contract) and G11 (logic <= 6 ms per tick).
2. G8: tile backend for menus/island -> island and town hall at 30 fps.
3. G9: combat HUD/logic cost + proper hardware-scaled "3D: Faster" with a 30 fps lock and a test.
4. G10: pet, concert, title and dialogue stages on the fast paths.
5. G12 (optional): 60 Hz presentation in tile mode.
Use the free ~28 MB of cartridge for precomputed tables and pre-tiled art.
EWRAM stays tight (10,120 B free after H11, floor 8,192 B); free RAM before spending it.

## 3. Priority 3: house finish

H2 (GBA room shells at pixel parity; clears 33 expected-fail frames), H3 (PS1
room shells), H7 (Hold the Door panel, guests, captions, PS1 black frame),
H11's code part is done (the GBA keeps and saves the house state; the real-house GBA run, `make -C gba test-home`, is still to do).

## 4. Priority 4: script

- Side quests: W6-W8 (side_a), W9-W11 (side_b), W12 free-play chatter,
  W15 (needs a decision), W16 integration.
- PG-21 punch-ups E6a-E6d (life-sim waves). Swearing budget for the whole game:
  the s-word a few times; the f-word exactly twice (once cut off, once in full near the end).
- Known gap: four choice questions outside Ch1-2 still need checking for "/" breaks.

## 5. Priority 5: languages (8 now: en, es, pt-BR, fr, de, it, nl, pl)

L2b formatted text, L3 tooling, L4 accented font, L5 story text for N
languages, L6 PS1 language tables on disc, L7 grammar-aware generators,
L8a-L8f Spanish content + polish (re-translate everything the rewrite touched),
L9 language picker, L10 pt-BR / fr / de / it (+ nl, pl). L11 docs: done (`docs/LANGUAGES.md`).
Later: Chinese and Japanese as languages 9 and 10 (need a new font path).

## 6. Priority 6: Dreamcast port + extras

DC3 PVR 2D, DC4 input + VMU saves, DC5 AICA audio, DC6 PVR 3D, DC7 overworld,
DC8 house renderer, DC9 full game + CI.
Extras: X0 contracts, X1 VMU live pet, X2 rumble, X3 VGA/display options,
X4 sharper 640x480 text, X5 keyboard + mouse, X6 VMU pocket pet,
X7 Dreamcast-only area + costumes, X8 boot easter egg, X9 4-player party,
X10 VMU card trading.

## 7. Working notes for next time

- Run life-sim waves back to back; for speed, run only the tests for the code a
  package touches, and keep the full run (`make -C core test`, `make -C gba test`,
  `make -C gba test-speed`, PS1 harness) for the end of each wave.
- Keep free disk >= 4 GB; delete landed worktrees.
- Messages to the session interrupt running agents; batch them.

## Key decisions (summary)

- Tone: The Walten Files is the main parody target; Princess Robot Bubblegum and
  Poochee & Pansy are tone references; indie-art parody is the backbone; abrupt
  laugh-out-loud comedy with a few cryptic beats; not a full ARG.
- Show canon in the game: turkey Mayor, "Needs salt", Bonejangles as a possessed animatronic.
- Register PG-21: adult comedy, implied/fade-to-black only, adults only for
  anything suggestive, never minors; mature art comes only from user-supplied
  override packs into reserved slots.
- Show plan: 5 seasons, Sunny dies in S4; the game keeps its own story where Sunny lives.
- Show swearing curve (the game keeps its own sparse budget: s-word a few times, f-word exactly twice):
  - Season 1: no f-word; it almost drops once and gets cut off.
  - Season 2: the first full f-word, near the end of the season.
  - Season 3: f-words here and there.
  - Season 4: ramps up hard, especially around Sunny's death early-to-mid season.
  - Season 5 (double length + TV-movie finale): constant, Vivziepop-level
    (Hazbin Hotel / Helluva Boss) swearing in frequency only.
  - Craft rule (all seasons and the game): every swear has a purpose: a punchline,
    a character beat or a real emotional hit. No filler swearing; that
    sloppiness is part of what the show parodies.
- GBA cartridge: 256 Mbit / 32 MB (the hardware maximum), enforced at build time.

## Patch workflow (Termux) and the next patches

One zip per patch in `~/storage/downloads/`, unpacking to a same-named folder with `apply.sh` (run from the repo root,
safe to run twice, no network). Use a NEW name for every patch. **The kind of command to send the owner** (replace NAME and
MESSAGE; it pulls first, unpacks into a scratch folder, applies, commits and pushes; ~/vs is left in place and the next patch's `rm -rf ~/vs` clears it):

    cd ~/DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark && git pull --ff-only && rm -rf ~/vs && mkdir ~/vs && unzip -qo ~/storage/downloads/NAME.zip -d ~/vs && sh ~/vs/NAME/apply.sh && git add -A && git commit -m "MESSAGE" && git push

Worked example (U6c):

    cd ~/DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark && git pull --ff-only && rm -rf ~/vs && mkdir ~/vs && unzip -qo ~/storage/downloads/doomo-u6c-continue-island-nov02f.zip -d ~/vs && sh ~/vs/doomo-u6c-continue-island-nov02f/apply.sh && git add -A && git commit -m "U6c: Continue row also leads with a saved island" && git push

| Patch | NAME | Commit message | What |
|---|---|---|---|
| U6a | `doomo-u6a-continue-first-nov02b` | `U6a: Continue-first title` | already in the tree |
| U6b | `doomo-u6b-quick-island-nov02c` | `U6b: quick island start (J9 13 -> 11 presses)` | this patch |
| U6c | `doomo-u6c-continue-island-nov02d` | `U6c: Continue row also leads with a saved island (J9 target 10)` | this patch: with no story save but an island save, the lead row continues the island; try one fewer welcome-dialog press |
| U6d | (none) | (no patch) | `creator_hellvile` already passes; the note is dropped by U6e |
| U3c | `doomo-u3c-island-quicksave-nov02g` | `U3c: START, START quick-saves the island (J7 7 -> 2 presses)` | this patch |
| U3b | `doomo-u3b-story-quicksave-nov02h` | `U3b: START, START quick-saves the story (J6 9 -> 2 presses)` | this patch |
| PS1 | `doomo-ps1-lifesim-checklist-nov02i` | `docs: PS1 life sim status checklist` | docs only: adds `docs/PS1_LIFESIM.md`, links it from the roadmap focus note |
| PA2 | `doomo-pa2-hero-cards-nov02d` | `PA2: hero cards finished (tagline fit, header-byte test, creator script fix)` | applied by the owner 2026-11-02 |
| PS1 | `doomo-ps1-journeys-nov02j` | `PS1: harness plays the press journeys (J6, J7) on the disc` | PS1 focus step 2: `run.py` handles `journey-begin/end` and checks `budgets.tsv` presses; game harness target runs J6 + J7; not yet run on PCSX-Redux |

How the owner applies patches (so nobody has to ask again): from Termux, one pasted line per patch, oldest first, each line
is the full template above (`cd` repo, `git pull --ff-only`, `rm -rf ~/vs && mkdir ~/vs`, `unzip -qo` the zip from
`~/storage/downloads/` into `~/vs`, `sh ~/vs/NAME/apply.sh`, `git add -A`, `git commit -m`, `git push`). No cleanup step at the end.
Rules for whoever builds a patch: a NEW zip name every time (`doomo-<id>-<what>-nov02<letter>`; the letter keeps counting up),
the zip holds one folder of the same name with `apply.sh` in it, `apply.sh` runs from the repo root, is safe to run twice
(guard with `grep -q` or `git apply --reverse --check`), needs no network and no python (sed, grep, cat, git apply only),
and the reply gives the line with NAME and MESSAGE filled in, plus a ROADMAP row (the apply.sh adds it) for each patch.
