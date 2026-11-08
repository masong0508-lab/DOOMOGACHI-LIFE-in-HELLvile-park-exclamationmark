# Held-off work: roadmap and restart guide

Written 2026-11-08, when the owner paused new work at 96-97% of the weekly usage limit
("finish speed, upgrade the controls and sticks and then stop"). Everything below is
**planned but not started, or started but not landed**. It is meant to be enough on its
own to restart each wave after the limit resets, in the owner's order.

Companion files in `docs/design/held/`:

| File | What it is |
|---|---|
| `OWNER_DECISIONS.md` | Every owner decision so far (tone, safety rules, FPS target, work order, levels answers, swearing curve, Talk hug, usage stop). **Latest entry wins.** |
| `SPEED_AUDIT2.md` | Speed audit 2: every non-combat scene measured on GBA/PS1/DC, the S2-x packages and the gap-closers. |
| `STICKS_PLAN.md` | ST1, real analog stick support (PS1 DualShock + Dreamcast). |
| `CREATOR_PLAN.md` | The creator plan: CR0-CR14, 1,566 new parts in art batches A1-A10+. |
| `patches/fut-tk3.mbox` | **Unlanded** Talk TK3 (T5a generator + T7 census + K-10 grief banks), 7 commits. |
| `patches/fut-dc8.mbox` | **Unlanded** Dreamcast DC8 (the house on the PVR), 9 commits. |
| `patches/fut-crc.mbox` | **Unlanded** creator CR3 (gene lists to live counts), 1 commit. |

Other plans already in the tree: `docs/design/talk/TALK_DESIGN.md` (Talk, T0-T10, TP0-TP4,
TD0-TD3), `docs/design/levels/LEVELS_EXPANSION_PLAN.md` (24 new levels, LV0-LV38),
`docs/DREAMCAST.md` (DC2-DC8 as built), `ROADMAP.md` (done / to do).

---------------------------------------------------------------------------

## 0. Order and rules (owner, 2026-11-08)

**Work order v2 + sticks:** clarity (done) -> speed -> **sticks** -> Talk -> Dreamcast ->
creator -> new levels. **One wave at a time**: agents work on the wave closest to completion;
an in-flight landing finishes before the next wave starts.

Rules that apply to every package below:

* **Steady 30 fps on every screen and platform** (GBA, PS1, Dreamcast). Combat is the one
  exception (SP8 left GBA combat at full-speed logic, ~15-17 drawn fps). A package that
  cannot reach 30 lands what helps and reports the measured number and the gap; it is never
  called done at a lower number.
* **Comprehensibility first**: every new screen has a plain one-line instruction and a
  control card; new mechanics are taught in a safe spot before they matter.
* **Content limits (never weakened):** PG-21 adult comedy register; no sexual content or
  genital nudity; anything suggestive is adults-only and minors are never in it; gore only on
  adults and demons, cute filter around kids, no harm ever shown to a child; no real-world
  hate; self-harm only handled with care, never a mechanic or a joke; `mature_adult_ok` and
  the kid-cast list are never weakened. Talk: adults-only romance in both directions,
  refusal/deflection with minors, no teen-teen romance; a friendly hug toward a child stays a
  plain hug the child may decline.
* **Memory gates:** PS1 EXE (built with the boot FMV linked, `-DDOOMO_INTRO_FMV=ON`) ends at
  least **72 KB below the stack top** (it is ~67-70 KB as of `d875604`: a small PS1 RAM
  package is owed); GBA EWRAM ~3 KB free, IWRAM near full, ROM limit 32 MB.
* **Gates before every push:** `make -C core test`, SAN build, `check-gen` byte-identical,
  `xcheck`, GBA build + tests, PS1 build + harness (`ps1/harness/run.py`), Dreamcast KOS build
  + `dcrun` when `dc/` is touched; docs + `ROADMAP.md` updated.
* **Landing pattern ("fast-land"):** each package is built in its own worktree on a `fut-*`
  branch -> reviewed (blocking claims verified with probes) -> rebased onto
  `claude/nice-allen-gr6czm` -> re-verified -> pushed fast-forward. Never force-push; never
  push to another branch.

---------------------------------------------------------------------------

## 1. Status at the stop

| Wave | State | Landed | Left |
|---|---|---|---|
| Clarity | **done** | CL1-CL6 | - |
| Speed 1 | **done** | SP1-SP8, Options volume freeze fix (`7370263`), PS1 RAM fix (`d875604`) | SP8 G-C2 (GBA BG2 2x + HUD as OBJ) not built |
| Speed 2 | **running at the stop** | S2-1, S2-2, S2-3, S2-6, S2-7, S2-8, S2-12, S2-13 (tooling) | S2-4, S2-5, S2-9, S2-10, S2-11, gap-closers G2-EX/CO/CR/PET, PS1 RAM package (see section 2) |
| Sticks (ST1) | **running at the stop** | - | all of `STICKS_PLAN.md` (section 3) |
| Talk | held | TK1 (T0-T2), TK2 (T3+T4) | TK3 landing (patch saved), TK4, TK5, TKP, TD (section 4) |
| Dreamcast | held | DC3, DC4, DC5, DC7 | DC8 landing (patch saved), DC6, DC9, DC5b (section 5) |
| Creator | held | CRA (CR0/1/2/12), CRB (CR6/7/9) | CRC (CR3 patch saved + CR4 first art), slice 2 (section 6) |
| New levels | held, planned | plan + owner answers | LV0-LV38 (section 7) |

Whatever landed after this file was written shows in `git log` and `ROADMAP.md`; check
there first.

---------------------------------------------------------------------------

## 2. Speed wave 2 (finish)

Owner: "speed up everything else please, it needs to run flawless (don't mind about the
combat if you can't do anything else about it)". Full measurements and package text:
`held/SPEED_AUDIT2.md`. Workflow script: `speed-wave-2` (run `wf_e29f8ad8-b98`); it resumes
from its cache with `resumeFromRunId` while the session lives; after that, build the
remaining packages one by one with the fast-land pattern.

| ID | What | Why (measured) | Done when |
|---|---|---|---|
| **S2-4** | Static backdrops kept: Fun Fair stage, kitchen, dream rooms/skies, cutscene card and vignette backgrounds, library lobby + jukebox, studio STAGE draw their unchanging backdrop once and keep it | these redraw full-screen art every frame on GBA (below 30) | each listed scene 30.0 on GBA and PS1 in the speed2 scripts |
| **S2-5** | Pre-scaled art in ROM: Derby racers (`mg_blit2` 2x), Vapula vignette sprite (`vid_blit_c` 39%), weapons-wing icons, cutscene 2x portraits | per-pixel scaling dominates those frames | 30 fps; ROM growth listed in the GBA ledger |
| **S2-9** | Workshop: weapon composition (`weld_gfx` outline + seg, 260 ms in one tick, 3x per visit on GBA, 100 ms on PS1) sliced across ticks with the old picture shown meanwhile | 19-23 fps and 260 ms freezes | no frame over 34 ms in `workshop` script |
| **S2-10** | Saves and loads without a frozen screen: free-play save streamed a few KB per tick (GBA flash sector erase+program per tick, PS1 memory card 128 B sectors per tick), a saving card; short fades over title -> New story and new-island overlay loads | GBA save 11.4 s in one frame; PS1 11.6 s; New story 2.3 s + 1.1 s frames | no single frame over ~100 ms; save still atomic (power-loss test) |
| **S2-11** | House leftovers: view-turn recompose (106 ms) built over 2-3 ticks in the hidden page; busy Live layout; creator hat try-on | one 117 ms frame per turn; 28.6-28.9 fps | 30.0 steady in `room-test` 1 and creator try-on |
| **G2-EX** | Exorcism gap-closer | GBA exorcism 22.5 fps after SP7 | 30 or the measured gap reported |
| **G2-CO** | Concert gap-closer | GBA concert ~20 fps after SP6 | same |
| **G2-CR** | Creator held-input gap-closer | GBA creator ~22 fps with input held | same |
| **G2-PET** | Pet scene last frames | ~29.5-30 with hitches | no frame over 34 ms |
| **PS1-RAM** | Give back ~2-5 KB resident on PS1 (move more cold code to `-Os` via `DOOMO_PS1_COLD`, or trim tables) | EXE ~67-70 KB below stack top, gate is 72 KB | >= 72 KB below stack top with FMV linked, no fps loss |

Pixel-changing steps (saving card, load fades, stand-ins, slice ticks) were pre-approved
under SPEED DEFAULTS; list each with before/after screenshots.

---------------------------------------------------------------------------

## 3. Sticks (ST1) - finish whatever did not land

Owner: "add actual dualshock/stick support so I don't have to move the camera controls with
wacky controlling". Full spec: `held/STICKS_PLAN.md`. Priority inside the package:

1. PS1: DualShock switched to analog mode by the game (config 0x43 / 0x44 lock / exit) at
   boot and on hot-plug; L3/R3 mapped. Controls default **Auto** = dual stick when an analog
   pad is present, Modern otherwise; explicit picks win; old saves on Modern migrate to Auto.
2. Right stick = camera in the house view and creator preview; left stick = analog-speed
   walking in the overworld/island.
3. Dreamcast: stick X turns, Y walks, analog triggers strafe (pressure = speed); Options row
   "Stick: turn / strafe".
4. Prompts and control cards show stick glyphs when an analog pad is present; the "press
   ANALOG" toast goes.
5. Every other camera from the audit (fishing aim, minigame cursors, 3D level select).

D-pad and buttons keep working everywhere; the GBA is unchanged. Gates: unit tests for
scheme resolution, dead zones/curves, DC trigger strafe and each consumer; harness runs with
injected analog values; 30 fps on touched scenes; PS1 72 KB gate. Work branch: `fut-st1`.

---------------------------------------------------------------------------

## 4. Talk (the Facade-style live house conversation) - PS1 + Dreamcast

Owner: talk/chat with created or resident characters in the pseudo-3D house, several people
at once, a word network producing ~1,000,000 different conversations with many
personalities; scratch it on PS1 if it doesn't work. Full design:
`docs/design/talk/TALK_DESIGN.md` (sections 9 and 10 have the package table).

**Landed:** TK1 = T0 (contract, `talk.h`, `PLATF_TALK`, save tag), T1 (word network +
`talkc.py`, `TALK_en.BIN` 503,324 B), T2 (`talk_db.c` reader). TK2 = T3 (parser,
`talk_lex.c` + `talk_parse.c`, 228 rules, corpus >= 1,500 lines, accuracy bars met) + T4
(`talk_safe.c` gate, safety corpora, precision fixes).

**Saved, not landed - TK3** (`held/patches/fut-tk3.mbox`, base `cd80654`): T5a generator
(`talk_gen.c`, `talkc_gen.py` grammar compiler, 668 starter EN alternatives covering every
act in the cast's voices), T7 census (`talk_census.c`, three tiers, Chao1, Q1-Q6, Q7 review
sheet, `talk_sample.py`), K-10 grief and comfort banks (grief is never a punchline), docs.
The reviewer's blocking claim (K-10) was verified and fixed; what remained was rebase ->
verify -> push. **Restart:** `git am` the mbox on a fresh branch from the current tip (or
`git am -3` after a rebase), resolve `ROADMAP.md`/`docs/TALK.md` conflicts, run the gates,
push.

| Package | Contents (design IDs) | Gates |
|---|---|---|
| **TK4** content | T5b: ~6,000 EN alternatives written to SCRIPT_STYLE (PG-21 adults, kid-safe kids, every cast voice), T6 drama manager (`talk_dm.c` beats, tension arc, responders, interruptions, alliances, walk-outs; `talk_outcome.c` capped outcomes into the sim; `talk_save.c` TALK chunk), then the full census | **Surface >= 1,000,000, Skeleton >= 100,000, Meaning >= 5,000, conversations >= 1,000,000, quality failures <= 1%**; TK1-TK9 lint clean; safety property tests P1-P8 at 0 findings; no farming over 100 sessions |
| **TK5** house mode | T8 `home_talk.c` (`HM_TALK`: gathering and seats, camera shots, bubbles via `plat_room_project`, portrait inset, emotes, quick replies, catch-the-word, the composer, typed line, invites), `room_ref.c` zoom 2, runner `talk-*` commands; T9 pointer input | `home_talk_basic/group/safety/visit` scripts; press budgets (quick reply 1, compliment next person <= 4, catch-and-ask <= 3); 30 fps native |
| **TKP** PS1 | TP0 re-measure; TP1 `talkdisc.c` (TALK_xx.BIN staged into SPU RAM 0x30000-0x7DFFF, SFX capped to 0x2000-0x2FFFF); TP2 overlay parts `talk` + `talkgen` (<= 12,288 B each, resident delta <= 4 KB, generation <= 8 ms/frame); TP3 zoom-2 framing, GTE `plat_room_project`, mouse; TP4 optional SPU part cache | **72 KB margin**; text parity with native (`expect-talk-hash`); load <= 1.5 s; **steady 30 fps** (owner override of the design's 28/24); every SFX still plays. If it cannot fit, the owner said scratch it on PS1 |
| **TD** Dreamcast | TD1 `/cd/TALK_xx.BIN` into heap; TD2 typed line from the DC keyboard, pad fallback, HD-text bubbles (40 cols); TD3 `dcrun` Talk scripts + frame-time gates | needs DC8 landed (section 5); scripts pass twice byte-identical |
| T10 (later) | other languages (ES pilot first); languages without a bank fall back to English with a notice | per-language census and safety corpora |

Talk safety (from the design, section 7; never relaxed): adults-only romance both ways;
minors get refusal/deflection; no teen-teen romance in Talk; typed self-harm content gets a
gentle out-of-character line encouraging the player to talk to someone they trust, then Talk
continues calmly; a friendly hug toward a child stays a plain hug and the child may decline.

---------------------------------------------------------------------------

## 5. Dreamcast port

Owner approved building the Dreamcast build and the Flycast libretro core
(`/opt/toolchains/dc/flycast_libretro.so`, rebuilt with `sh dc/harness/build_flycast.sh`).
Toolchain: KOS in `/opt/toolchains/dc`; harness `dc/harness` (`dcrun`). As built:
`docs/DREAMCAST.md`.

**Landed:** DC2 skeleton, DC3 PVR 2D, DC4 input (pad, keyboard, VMU, analog feel), DC5 AICA
audio, DC7 overworld in 3D on the PVR (`dc3d.h` batches, twiddled textures).

**Saved, not landed - DC8** (`held/patches/fut-dc8.mbox`, base `18be42b`): `dc/src/room.c`
(`plat_room` on the PVR: the dream house), GLASS/ADD 2D modes, host parity test against
`room_ref` (`test_dc_room.c`), `make -C dc room-test` on Flycast, review fixes (invalid ghost
red, glows under shades), DREAMCAST.md section 8, RAM re-measured. It was mid-landing (rebased
and re-verified once). **Restart:** `git am -3` onto the current tip, re-run `dc-host-test`,
`room-test`, core gates, push.

| Package | What | Gates |
|---|---|---|
| **DC6** | `plat_view3d` (the first-person Hell Breach / combat view) through DC7's `dc3d` batches; honour `OPT_DRAW_DIST` and the combat detail option (Smooth / Balanced / Max detail, adaptive governor) | combat scripts on `dcrun` at 30 fps (DC has the headroom: aim for Max detail at 30); parity shots vs PS1 |
| **DC9** | Full game on Dreamcast: every scene reachable, disc image (`mkcdi.sh`), VMU saves end to end, the boot FMV, all harness scripts (story, free play, side missions, minigames, house, creator) passing | full `dcrun` suite twice byte-identical; frame-time table for every scene; DREAMCAST.md + BUILDING + ROADMAP |
| **DC5b** | Music engine's IRQ tier on the Dreamcast (left open by DC5) | audio-test envelopes vs native |
| ST1 part 3 | Stick + analog trigger scheme (section 3) | - |

---------------------------------------------------------------------------

## 6. Creator: 1,500+ new parts and a better screen

Owner: at least 1,500 new parts and accessories; improve the creator screen further; 30 fps.
Plan: `held/CREATOR_PLAN.md` (1,566 new parts + 45 already-drawn extras; CREATOR DEFAULTS in
the decisions file: the Walten Files parody set is an original send-up, mild adult-only props
tagged adult and never on minors, no tobacco/vape/drug props, about a third of themed sets
locked behind unlocks).

**Landed:** CRA = CR0 (budgets/baselines), CR1 (part catalog, 433 parts catalogued), CR2
(renderer for many parts: u16 lists, slot sets, VM macro ops), CR12 (age gate). CRB = CR6
(browser grid picker, sections, filters, search, try-on camera, locked overlay, help), CR7
(prerendered 20x20 part icons), CR9 (speed: sliced looks, retained panels; GBA held input ~22
fps, see G2-CR).

**Saved, not landed - CR3** (`held/patches/fut-crc.mbox`, base `38a362d`): the gene lists grow
to their live counts; the new look fields get creator rows, reach the renderer and persist.
CRC's remaining step was CR4's first art (524 gene-width parts). **Restart:** `git am -3`,
re-run `test-creator` on GBA/PS1, then CR4.

| Package | What | Gates |
|---|---|---|
| **CRC** (finish) | CR3 (saved) + CR4 gene-width art (524 parts) via part scripts + catalog + contact sheets + shape gate + `review.txt` | core test, SAN, xcheck, check-gen, GBA `test-creator`, PS1 harness; contact sheets reviewed; 30 fps |
| **Slice 2** | CR5a GBA EWRAM room (island hub record lists into the unused arena region +12.5 KB, or a 4 KB GBA-only look pool, only if the island hub stays at 30); CR5 deep parts saved (real LOOK pool/chunk); CR10 PS1 memory (SPU-RAM part store or `PARTS.BIN`); the rest of CR4's art batches (A1 face & eyes 283, A2 hair 148, A3 spooky 134, A4 fur & body 54, A5 outfit 224, ... to reach >= 1,500 new); CR8 outfits and presets; CR11 set unlocks (~120); CR13 prefs chunk; CR14 docs and part names in the language packs | live count >= 1,500 new parts; old saves load; age gate test; 30 fps on GBA/PS1 with input held |
| Levels sets | Six new creator sets tied to the new levels (owner: "yes") | per LEVELS plan |

---------------------------------------------------------------------------

## 7. New 3D levels: 9 main-game + 15 New Game+

Full plan: `docs/design/levels/LEVELS_EXPANSION_PLAN.md` (story, every level's layout and
mechanic, dark-level handling, the GBA's 7, fps budgets, the package table LV0-LV38 in its
last section) and its "Owner answers (2026-11-08)" section.

Owner decisions: PS1 and Dreamcast get all 24; **the GBA gets 7**; about 3 of 9 main and 6 of
15 NG+ levels are darker in mood and topic, each opening with a content note; the NG+ story is
bombastic, complete and grows more mature from the late-middle to the end; one or two dark
levels open NG+ (the cold open autoplays); NG+ is Chapter 5 with endings E03/E05/E07; memorial
names shown by default; six new creator sets; the Duchess becomes datable in the good ending
only with a long, demanding adults-only route. The game keeps its own sparse swearing budget.

Build order (from the plan):

1. **Foundations LV0-LV11:** level registry and records (`LV_COUNT` 31); map format v2
   (zones, paths, swap sets, texbank); level script VM (`lv_script.c`); mechanic kits A
   (movement), B (environment and light), C (rules and companions); enemies/AI/bosses (4 new
   base demons, `DemonSkin`); art and audio kit; platform delivery (PS1 `BREACH.BIN` packs,
   VRAM pinning, overlay groups; GBA ROM banks; DC); combat detail option per level; test
   harness (`test_levels2.c`, `breach_scripts.py`); content notes and dark-level presentation.
2. **Main-game levels LV12-LV21:** HB8 Steam Room Panic, HB15 The Department of Contracts,
   HB13 Boneyard Buffet & Bandstand, HB9 The Undercroft (dark), HB11 The Glum Pipes (dark,
   PS1/DC), HB12 The Wall (dark, PS1/DC), HB10 Fright Night at the Fun Fair, HB14 Hell Pits
   Highrise, HB16 The Ninth Cloud; LV21 story hooks, side missions and rewards.
3. **New Game+ LV22-LV37:** the arc's story layer (Chapter 5, flags, choices); N1 Day One
   (dark cold open), N2 The Dead Letter Office, N3 Repo Night, N4 The Interest Engine, N5 Lake
   of Fire Regatta, N6 Everything Must Go, N7 The Audit (midpoint), N8 The Debtors' Prison
   (dark), N9 The Artist's Studio (dark), N10 The Ward of Forgotten Names (dark), N11 The
   Hollow Mingle (dark), N12 The Interest, N13 The Vault of Unfelt Things (dark), N14 The
   Ledger (finale, three endings), N15 Both Are Better (epilogue).
4. **LV38 integration:** all 24 on every harness, the fps table, Boss Rush "Arrears", docs.

GBA's 7 (section 7 of the plan): HB8, HB15, HB13, HB9, N1, N7, N14 (the others are PS1/DC).
Prerequisites: speed wave done, DC6 for the Dreamcast versions.

---------------------------------------------------------------------------

## 8. Older open items (from `ROADMAP.md`, not part of this week's order)

These were open before this week and stay behind the order above unless the owner reorders:
SP8 G-C2 (GBA BG2 affine 2x + HUD as OBJ for a 30 fps combat lock); the integration smoke
playthrough (`integration_smoke.txt`); GBA EWRAM >= 24 KB free and PS1 headroom >= 128 KB
(memory goals beyond the gates above); music MU4 cue waves and MU5b Studio LIVE; the show
pipeline lanes (art sets, SFX library, score cues). See `ROADMAP.md` for their exact state.

---------------------------------------------------------------------------

## 9. Restart checklist (after the usage reset)

1. `git fetch origin claude/nice-allen-gr6czm`; read `git log` and `ROADMAP.md` for anything
   that landed after this file.
2. If speed wave 2 or ST1 did not finish: finish them first (sections 2 and 3).
3. Land the three saved patches in order of the work order: TK3 (Talk), then DC8
   (Dreamcast), then CR3 (creator), each with `git am -3` on a fresh `fut-*` branch, gates,
   fast-forward push.
4. Then Talk TK4 -> TK5 -> TKP (-> TD after DC8), Dreamcast DC6 -> DC9, creator CRC -> slice
   2, new levels LV0 -> LV38, one wave at a time.
5. Keep at least 4 GB of disk free (delete build folders of idle worktrees).
