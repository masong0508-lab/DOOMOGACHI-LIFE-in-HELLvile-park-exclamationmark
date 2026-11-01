# DOOMOGACHI roadmap (what's left)

Last updated 2026-11-01. The decisions behind this list (tone, PG-21 register,
show canon, life-sim answers, 32 MB GBA cartridge) are summarised at the end.
Package IDs match the planning files; each package is built in its own worktree,
reviewed, then landed on `claude/nice-allen-gr6czm`.

## 0. Status at a glance

**Done and on the branch**
- Script: style guide + locks (P0), Chapters 1-4 + epilogue rewritten (W1-W4),
  the 49 old quests rewritten (W5).
- House: golden coverage (H1), themed doors/windows + Towers 7B / Hell Pits rooms (H4),
  garden/plot (H5), animations (H6), lighting (H8), back facings (H9), 57 furniture
  pieces redrawn + roofs, GBA EWRAM for HOME (H10). PS1 disc links again (H0).
- GBA speed: speed tour (G1), Mode 4 quick wins (G2), IWRAM overlay (G4),
  tile-mode overworld (G5), billboards as sprites (G6), retained HUD panels +
  "3D: Faster/Better" (G7). Cartridge is now a full 256 Mbit / 32 MB.
- Languages: N-language engine (L1), one text path everywhere (L2a).
- Dreamcast: SH4 cross-check (DC0a), KOS toolchain script (DC0b),
  headless harness (DC1, not yet run on real Flycast), dc/ skeleton (DC2).
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
- **S-HOUSE**: story unlocks the house; cap 3 -> 10 after completion -> 20 after NG+;
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
EWRAM stays tight (H11 needs 12,352 B free); free RAM before spending it.

## 3. Priority 3: house finish

H2 (GBA room shells at pixel parity; clears 33 expected-fail frames), H3 (PS1
room shells), H7 (Hold the Door panel, guests, captions, PS1 black frame),
H11 (turn the GBA house state on and test the real house).

## 4. Priority 4: script

- Side quests: W6-W8 (side_a), W9-W11 (side_b), W12 free-play chatter,
  W13 library flavour, W14 minigame/shop/unlock text, W15 (needs a decision), W16 integration.
- PG-21 punch-ups E6a-E6d (life-sim waves). Swearing budget for the whole game:
  the s-word a few times; the f-word exactly twice (once cut off, once in full near the end).
- Known gap: four choice questions outside Ch1-2 still need checking for "/" breaks.

## 5. Priority 5: languages (8 now: en, es, pt-BR, fr, de, it, nl, pl)

L2b formatted text, L3 tooling, L4 accented font, L5 story text for N
languages, L6 PS1 language tables on disc, L7 grammar-aware generators,
L8a-L8f Spanish content + polish (re-translate everything the rewrite touched),
L9 language picker, L10 pt-BR / fr / de / it (+ nl, pl), L11 docs.
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
- GBA cartridge: 256 Mbit / 32 MB (the hardware maximum), enforced at build time.
