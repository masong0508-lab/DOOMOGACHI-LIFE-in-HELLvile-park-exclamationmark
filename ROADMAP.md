# BOREagotchi roadmap (what's left)

Last updated 2026-11-06 (full refresh of done / playable / to do just below; the older lines below keep their own dates and are history). The decisions behind this list (tone, PG-21 register,
show canon, life-sim answers, 32 MB GBA cartridge) are summarised at the end.
Package IDs match the planning files; each package is built in its own worktree,
reviewed, then landed on `claude/nice-allen-gr6czm`.

## Refresh 2026-11-06: done, playable, to do

Checked on this tree with the native gcc build only (`make -C core test`: 861 unit tests, 160 scripted playthroughs, 19 press journeys at
240x160 and 320x240; `check-gen`, `story-check`, `langpack`, `mpack` clean). **PS1 and Dreamcast were not built or run in a chat session** (no MIPS/SH4
toolchain, no emulator); their statements come from the docs and your CI logs. **GBA can be:** on 2026-11-06 a chat sandbox with apt
installed `gcc-arm-none-eabi libnewlib-arm-none-eabi libmgba-dev`, built the game ROM and ran the real mGBA harness (`make -C gba`,
`make -C gba profile-invaded`), so GBA speed numbers can be measured in a session, not only read from your CI logs.

**Patches the owner applied that may not be in the zip a session was given:** `doomo-mp1-coop-perf-nov05` (MP1 part 5: co-op quests, rewards, abilities, family
portrait; its island-population-once-per-frame part is already in this tree), `doomo-perf-ram-nov05b` (apartments header count cached, `tools/ram_ledger.sh`,
`tools/perf_profile.sh`), and `doomo-p0-contracts-gene-roster-v2-nov05` (contract stubs, `look_build.py` gene table, roster v2 58 B records). Check `git log`
before redoing any of them; a zip without `.git` cannot tell.

### Done
- **Talk T0-T2 (TK1, host-tested; docs/TALK.md):** the Facade-style house conversation's contract (`talk.h`: acts, `TalkUtter`, the `TALK_<lang>.BIN` format; `PLATF_TALK` bit 9; zoom 2 talk framing + `plat_room_project`; the `'TALK'` save chunk, 320 B declared; `NT_TALK_*` news), the English word network (3,972 concepts: 1,317 generated from game data, 2,655 authored incl. 95 safety-only; 21,254 reviewed association edges; append-only `concepts.lock`), the compiler `core/tools/talkc.py` (register agreement with `register.tsv`, no edge from kid-safe into adult talk or from adult talk into a child word, every child word flagged TCF_MINOR_WORD, font + story-codec coverage, 15 seeded refusals) + `talk_assoc.py`, and the reader `core/src/talk_db.c` (TALK_en.BIN 219,672 B, PS1 subset 168,891 B of 312 KB). Nothing reachable in play yet: the T4 safety gate comes before any generated line. A friendly hug toward any child stays a wholesome hug the child may decline (owner "TALK HUG"; `TA_HIGH_FIVE` is just a gesture). Design: `docs/design/talk/TALK_DESIGN.md`.
- **Dreamcast sound (DC5, verified on Flycast and on the host):** `plat_tone`, `plat_sample` and `plat_sample_ex` on the Dreamcast (`dc/src/audio.c`, portable `mixer.c` / `sfx.c`): at the end of every logic tick the main thread renders the tick (four tone channels as musrender voices them, eight effect voices with pan, pitch and priority stealing, a DC block, a gentle low-pass, a soft knee near the rail) into one 1,470-frame slot of a PCM ring at 44,100 Hz, and a KOS stream thread copies slots to the AICA (6144 B per channel). The sound is a function of the game's ticks (a content hash per 600 slots repeats exactly). The synth is on the main thread because KallistiOS's 10 ms tick hands the CPU to the game thread for a whole slice whenever it lands in the stream thread's work, whatever the priorities: rendering in the stream callback took 11-13.7 ms in 3% of the calls and pushed AICA refills to 40 ms apart against a 46 ms buffer; now the worst refill interval is 45 ms against 70 ms. The music engine stays on the legacy bridge tier like every other port (the IRQ tier waits for a `DoomoVoice` renderer and S1's effects in IRQ mode; docs/DREAMCAST.md section 6 says what it needs). Host (`make dc-host-test`, CI with python3-numpy): 2,046 mixer checks (every effect at five pitches against a double-precision reference, after review found an int32 overflow in the interpolation that clicked in 28 of the 35 effects; the SAN run now fails on any UBSan report), and the mixer's WAV against musrender on fp_title / story_ch1 / action_combat / story_invasion, envelope r 0.97-0.999. Flycast (`make -C dc audio-test`, part of `make -C dc test`): title, invaded town and combat with sound against `runner --wav` (r 0.996 / 0.945 / 0.922, the same note in 86-94% of windows, level 0.95-0.97), 0 underruns and 0 late AICA refills, the title's hash twice; the mixer costs 1.3-1.5 ms a tick (4-5% of the 30 fps budget); DC3's ui-compare (94/94) and DC4's VMU checks still pass. Latency about 100 ms from the end of a tick to the speaker (music and effects together; the AICA-channel effects path was measured and not taken). Owed: the mixer cost, refill timing and latency on a real console (docs/DREAMCAST.md section 6).
- **Dreamcast controls and VMU saves (DC4, verified on Flycast and on the host):** the controller, analog stick (`input_set_analog` + the PS1's smooth feel; past 48/128 it also moves the menus), analog triggers past half, START+Y = SELECT, C/Z/D and a twin stick's second D-pad (L2/R2), and the Dreamcast keyboard (buttons, and typing straight into the on-screen keyboard) (`dc/src/pad.c`, portable `padmap.c`). Saves and the machine config record on the first VMU (`dc/src/vmusave.c` over the portable file system `vms.c`): VMS files with the save's own Boreagotchi as an animated icon and an eyecatch (`save_icon.c`), an atomic replace (written as `.NEW`, read back, then swapped by two directory writes; in place only on a card too full for two copies), partial reads for the menus' header peeks, the card's metadata and the three headers learned once at boot (the title never touches the VMU), a "Saving your game... Do not remove the VMU" card with a progress bar while a save blocks the tick (a 2D side frame), the config record written behind (on the next scene change), `plat_test_flags` through a new mailbox word (`dcrun --console` for a real console's first boot). Host (`make dc-host-test`, CI): `dc/tests/vmu` pulls a simulated VMU out before each of a save's 62 block writes and always loads the old or the new save whole; 128,615 checks, also under ASan/UBSan; `dc/harness/vmu_check.py` checks the card images independently. Flycast (`make -C dc vmu-test`, part of `make -C dc test`): saves over a power cycle (the 1000-citizen island, 124 blocks, and a story island; in-place rewrite on run 2), a full VMU, no VMU, the first-boot picker writing `DOOMOCFG.SYS`, and stick / trigger / keyboard shots byte-identical to the D-pad ones; DC3's ui-compare still 94/94 (two new known diffs: the Performance page's Draw distance row follows `input_feel()`). Measured (emulated): a 27-block save 3.05 s, the 124-block worst case 13.0 s, a load 0.65 / 2.5 s, boot VMU scan 262 ms; every non-save tick of the six DC3 scripts under 33.3 ms. Also fixes DC2's harness `town-fill`, which randomised gene bits no save carries (so `town-crc` never matched after a load). Owed: a real VMU (write speed, a pull mid-save), a real keyboard and twin stick, the BIOS file manager (docs/DREAMCAST.md section 5).
- **Dreamcast 2D layer (DC3, verified on Flycast and on the host):** every platform.h 2D call, the post effects and the held frame on the PVR (`dc/src/gfx2d.c`, `texcache.c`, `font.c`, `pal.c`, `pvr2d.c`): the 320x240 screen at 2x as one presorted translucent list, sprites with 16-bit UVs, merged rect runs, twiddled PAL8 textures with an LRU cache, glyph and outline pages, exact palette-space shading (`plat_shade_rect` re-draws what is under it through the colormap). `make dc-host-test` (also in CI) renders the lists with a software PVR next to the native runner: the six DC3 UI scripts match on 94/94 shots to the pixel, all 80 scripts 854/905 shots (mean 99.98%, post effects in software). On Flycast (`make -C dc ui-compare`, part of `make -C dc test`): the same six scripts on the CDI pass 94/94 shots against the native runner, 86 identical to the pixel and 8 with a listed platform difference (`dc/harness/known_diffs.tsv`: the DC corner label, the Rumble row, the pause menu's 3D level that is DC6's); every tick under the 30 fps budget except 3 one-off loading ticks in fp_screens (new island 96 ms, VMU save 187 ms, VMU load 254 ms; emulated time). `dcrun` gained tick-exact `sync`, `--native-seed`, the native runner's `press` and `type`, and waits for the mailbox before a command; the harness boots an ELF without debug info (Flycast cannot open one over 16 MiB). Owed: the frame time on a real console (docs/DREAMCAST.md section 4). Also fixes the KOS build of `dc/src/rumble.c` (deprecated Puru Puru fields under `-Werror`).
- **Creator parts: catalog, many-part drawing, age gate (CRA = CR0 + CR1 + CR2 + CR12 of the week-2 creator plan):** the part catalog `core/data/parts/catalog/` (35 slots, 433 parts: every live option, the 66 drawn-but-hidden ones, 25 themed sets, 31 tags, age classes, the plan's 1,611-part targets) compiled by `tools/partcat_build.py` into `part_meta` (names, sets, search, cost weights) and a resident age table; shipped ids frozen in `core/tests/golden/part_ids.txt`. The renderer takes any number of part-script sets with u16 list ids, slot hooks in every painter (a scripted value replaces the painter; tops / bottoms / shoes overlay), family macro ops (crown, brim, band, strap, bow, ring, chain, frill, pouch) and templates, a shape-only render mode and measured cost weights; the PS1 reads scripts through a 2-slot LRU + 4 KB per-render arena (same 8 KB) with an SPU-RAM source hook. First family batch: 12 hats, a sash, a satchel, a necklace (hidden until CR3 flips the lists). Age gate: teen / adult parts are never offered to, swapped off or drawn on a minor (creator, `look_for_stage`, generator, babies). `tools/part_budget.py` prints per-slot costs (all 1,611: about +480 KB GBA ROM, +146 KB PS1 non-resident). GBA (on `b080562`): ROM 4,301,668 -> 4,314,712 B, EWRAM 3,540 B free (0 change), IWRAM stack 6,356 B; GBA `test`, `test-speed`, the new `profile-creator` (fps unchanged). PS1: the VM is built off until CR10 (it would be ~22 KB resident); +1,888 B resident, disc 13,040 B above the stack reserve, harness `creator.txt` PASS, `perf_creator` 30 fps. Native `make -C core test` green (unit 905), check-gen, xcheck. Docs: docs/CREATOR.md (catalog, age gate, CR0 baselines), docs/PORTRAIT.md "Many parts (CR2)". Next: CR3 (flip lists live), CR4 art batches, CR5/CR5a deep parts, CR6 grid picker, CR7 icons.
- **Rename (2026-11-08, text only):** the game is now **BOREagotchi LIFE! in HELLvile PARK...** and the pet is a **Boreagotchi** (plural Boreagotchis; the Doomodex is the **Boredex**, the default pet name is "Bori"). Changed: every player-visible string in `core/src`, the story packs, the Spanish tables (the 6 other languages have no tables yet), the title logo (`fp_title.c`: 11 letters "BOREagotchi", a wider "LIFE!" pill, "in HELLvile PARK..."; fits 240x160 and 320x240), the p0 title card, credits, the pet LCD brand, the GBA header title `BOREAGOTCHI`, the PS1 disc volume / CMake description / memory-card titles / loading screen, the Dreamcast IP.BIN title and maker, and the doc headings. Unchanged on purpose: C identifiers and file names (`doomo_*`, `DOOMO_*`, `ITEM_DOOMOGACHI_EGG`), save magic and layouts, GBA code `BDME` / maker `DM`, build outputs (`doomogachi.gba`, `doomogachi.cue`, ...), boot log lines the harnesses expect (`DOOMOGACHI GBA: boot ok`), the repo name, and the trailer, show and pilot-proof sources (they draw their own logo art and need a re-render).
- **PS1 boot FMV fixed (fut-fmv, measured in PCSX-Redux):** the Dippinn ident (`INTRO.STR`) played as garbage on every disc: the picture squashed into the top 96 rows over two green bands. Cause: psxavenc `82f3871` scales a 16:9 source to `-s 320x240` "fit" = 320x192 but its MDEC encoder still reads those frames as 320x240 (the same garbage decodes on the host with ffmpeg, and in PSn00bSDK's own strvideo example). `ps1/CMakeLists.txt` now encodes `-s 320x176 -I` (16:9 fit to 16-pixel macroblocks, scaler and encoder agree); `fmv.c` also ends the intro on a frame larger than the screen. Captured frames vs the source: mean PSNR 9.5 dB before, 39.8 dB after (149 frames, audio unchanged and matching the source); the game boots after it and the PS1 harness passes.
- **Home Score window (nov07z3, CI fix, exact, unmeasured on the GBA):** `home_draw` now names the Score window body (inset 4 from the corners, from the shared `score_box`) to the room renderer BEFORE `plat_room` while the panel is fully popped, so `gba/src/room.c` skips its per-frame check and restore under it. CI `home_modal` was 17.5 drawn fps against a floor of 18 (51 ms a tick). Test `core/tests/test_home_score_opaque.c` (3 tests); `make -C core unit` 881 passed. Check on CI: `test-home` passes (`home_modal` >= 18) and every `vhash-seq` is unchanged.
- **Island town hall menu (nov07w, exact by construction, unmeasured on the GBA):** `draw_menu` dims only the four bands around the fully popped window instead of the whole screen first (`vid_shade_words` was ~10% of the scene, about 70% of that area is under the opaque window); check the CI "island town hall menu" row (expect `vhash-seq` still fe75623b) and the next `profile-all` top functions.
- **Speed (nov07z2, exact, unmeasured on the GBA):** `fx_dist` split root (top half from the 32-bit root, last bits one at a time; test `fixmath_dist_split_root_is_exact` vs the old root, 400k samples) plus `fx_dist_lt` / `fx_dist_gt` (no root) at the compare-only collision sites in actor / combat / bot / arena / levels; LYRICS parses each line once a frame; pet: shadow drawn per run not per pixel, egg half widths cached per shape. Check on CI: combat, pet and LYRICS rows of the speed report.
- **Choir caps (nov07z):** `SONG_SINGERS` (`core/include/doomo/song.h`) is per platform: GBA 16, PS1 24, host 25 (so the 877 host tests still run the full 25). The choir grid (`studio_words.c`) is 3 rows computed from it (16 = 6/5/5, 24 = 8/8/8, 25 = 9/8/8), the stage-row cursor value, `%d/N` counter and the "both districts" / park-template fills follow it, and `STG_N` in `gba/src/obj.c` is 16 (OBJ VRAM for the choir shrinks from 25 to 16 chibis). The Concert Hall choir picks up the same caps. Old saves with singers in slots above the cap are not kept (owner said no save compatibility). GBA / PS1 not built here: host tests and `-fsyntax-only` with both platform defines only.

| Area | What is finished (core, host-tested) |
|---|---|
| Life sim | **care screen decluttered: Gift and Heal are one "Give" bag (presents first, then remedies), Advice only shows when the citizen has something to advise on, no action listed twice, and Edit look / Favourite / Their home live under one "Manage" entry** (lifesim core 2b-2c); **creator START menu lists only what applies (no greyed Undo / Redo / Reset), one Randomise entry with an all / this-tab switch, grouped with rules and icons** (creator 1); **fast-forward catches the town up hour by hour** (the story bed / travel: a night now rests the sleepers and is not paid as eight breakfasts and showers), the edge pass no longer steps over a dropped acquaintance, best friends / nemeses hold their rank (hysteresis 60 / -60) (lifesim core 2); family table + pool, family screen + add-member wizard, care-all (F1-F2); house occupancy (F3); ageing, births, family events (F4); pets, old age, the late-game uplift (F5); loss, memorials, flowers (F6); free will, bonds, moods (F7-F8); helpers, away trips (F9); disasters + story hooks (F10); life events (LS3-LS4); household tiers + New Game+ (S-HOUSE tiers part); news, quick care (LS-QA) |
| Play as | play-as and New life (PA1-PA9), hero choice cards (PA2), profile guests and the picker, typed names, renaming, the Free mode gate (MP0 core, PA10 parts 3-4b) |
| Co-op (MP1) | co-op heroes Ember / Rook / Wick with final names and looks (part 1-2), `docs/COOP_HEROES.md` content, Ember's Ch1 quest scene + trigger (parts 3-4); part 5 (quests, rewards, abilities, family portrait) delivered as a patch |
| Story + content | chapters 1-4 + epilogue (W1-W4), 49 old quests rewritten (W5), about 109 live side quests, ending engine with routes and secret endings (E1-E4, E12), PG-21 adult beats in Ch1-2 (E6a/b), content-pack format + readers (E2, E7), mature-pack adapters for GBA / PS1 / Dreamcast (E8-E10, host-tested only) |
| Title, settings, UI | Continue-first title, quick saves, island START menu, held-frame backdrop (U6, U3, U4), settings core + options redesign (U0, U2), first-boot language picker + Mature gate (U5, E11), streamlined New Island (LS6) |
| House, shooter, extras | house Live / Build / Photo + HomeMart + Black Market + Hold the Door (H-series, GBA EWRAM H10-H11), Hell Breach + arena + welding workshop, 7 Fun Fair minigames, Concert Hall, Music Studio, Library, fishing, cooking, 8 languages in the engine (Spanish furthest) |
| Customization | gene codec, age = life stage, copy-out of all look sites (P0.M0a), 13 wave-2 face/head painters + 6 halos (P1.M2a-e); none selectable in the creator yet |
| Speed and RAM (host-checked) | GBA speed tour, Mode 4, IWRAM overlay, tile-mode overworld, billboards as sprites, retained panels + 3D Faster/Better (G1-G7); island population counted once per frame; apartments header cached (nov05b); `tools/ram_ledger.sh` and `tools/perf_profile.sh` to find the next ones |

### Playable now

| Where | State |
|---|---|
| Native runner (core) | everything in the Done table: Free Play (empty or 1000-citizen Classic island), creator, apartments, care, shop, news, family, pets, loss + memorial, New life, NG+, Hell Breach, the 4-chapter story + epilogue, house, minigames, library, concert, studio, 8 languages. 69 scenes, none a weak placeholder |
| GBA | CI builds the ROM and runs mGBA tests (green on run #470); **invaded map measured 2026-11-06: 14.9 drawn fps, 29.8 logic ticks/s, work avg 56.8 / max 76.5 ms, sprites ~40% of the frame (`docs/INVADED_SPEED.md`)**; EWRAM is nearly full (about 10-13 KB free) and fps is low: island hub ~7.5, town hall 6.5, combat 10-12, pet 10, concert 13, title 12-14. Press latch and hub B shortcut not yet checked on hardware (`docs/HARDWARE_CHECK.md`) |
| PS1 | CI builds the disc; house room shells were measured earlier (Hell-Cellar 30 fps); the harness re-run, config file, J6/J7 on the disc and the family UI on console screens are not confirmed |
| Dreamcast | skeleton + the whole 2D layer (DC3) + controls, keyboard and VMU saves (DC4) + sound (DC5), verified on Flycast; no 3D yet |
| Not selectable yet | deep customization (M2 painters, M3), mature pack art (loaders exist, PS1 has no stills) |

### To do, in the order to take them

1. **Verify on the consoles (the biggest gap).** Run `docs/HARDWARE_CHECK.md` on GBA and PS1, the PS1 harness (`ps1/harness`), the GBA fps test, and measure EWRAM / PS1 RAM after MP0, MP1 and the nov05b change (profiles + co-op code added about 1.5 KB code and 700 B of statics everywhere). Needs the owner or CI; nothing here can substitute.
2. **PS1 focus (standing instruction, see "Current focus").** PS1 budgets in `docs/PS1.md`, `docs/PS1_LIFESIM.md` checklist, PS1 RAM overlay plan.
3. **Life sim core, open parts:** (PA10 part 2, per-avatar pockets, is done: `docs/PLAYAS.md` section 9, `pa10_*` tests); part 3 New life from the title and in a fresh world; MP1 left: quests 2-4 on later days, Rook's and Wick's quests, local simultaneous multiplayer, helping in another world; the S-HOUSE story house unlock (**needs your definition**); polish (uplifted pet look, baby outfits and body, elder stoop, a household graveyard scene); hand-written disaster scripts (**need your tone check**).
4. **GBA speed and RAM:** **invaded (in progress, target 20 fps with 30.0 logic ticks/s):** V0 and V1 (GBA side) are done; steady frames are 50-60 ms so about 8 ms must come off the average (sprites are ~23 ms of it); V2 (tint flip) is demoted (`lit_cached` 0.7%); V4a done (sprite tables kept across frames: invaded 57.8 -> 54.5 ms, 14.9 -> 15.6 fps, hellvile -1.6 ms, all 16 scenes pixel-identical, EWRAM free now 24,240 B); V4b landed (cheaper `ow_sprite` setup, mirrored-row fast path: invaded 54.5 -> 52.0 ms, 15.6 -> 16.2 fps on the CI tour; parity vs V4a still to confirm, invaded `vhash-seq` 5e74560c); V4b confirmed on libmgba (52.0 ms, 16.2 fps, `vhash-seq` 5e74560c); spike frames explained: each is one chibi body-frame render when a citizen turns to a new facing, not the tint flip, and removing them is worth ~0.2 ms (94 of 115 steady frames are over the 50 ms a 20 fps frame needs, median 53.1), so the road to 20 fps is the steady frame (`docs/INVADED_SPEED.md`): V5 toast / HUD closed (the only toast is a one-shot hint before the measured window); steady split after V4b: sprite rows 15 ms, ground ~10, logic ~6 (two ticks a frame), so exact steps alone cannot reach 20 fps here and the owner decides between V6 (logic every second tick, ~3 ms), non-exact detail cuts (far billboards, far ground), or moving the target (keep ~16 fps and make logic 30.0 / max ~62); **V6 landed 2026-11-07 (owner chose it): the A-prompt search (`find_interactables`) runs every second tick on the invaded map only, ~1.8 ms expected, unmeasured on the GBA, `ownpc_update` left alone because thinning it would slow the citizens; V6 measured on CI: nothing (52.0 -> 51.9 ms); V8 landed 2026-11-07 (body look hash memo in `bodyspr_image`, exact, ~1.5 ms expected, unmeasured on the GBA); V8 measured on CI: invaded 51.9 -> 50.0 ms, 17.3 fps, exact; V9 landed 2026-11-07 (side_spots memo, exact, ~1 ms expected, unmeasured on the GBA); V7 (hardware billboards) was reviewed and not started: G6's own tile-frame numbers lose to Mode 4 on every map, so it is expected to give ~20 fps at best, not 30**; each step keeps the invaded `vhash-seq` frames identical. Then, for the other scenes: first read the CI run's Summary tab ("GBA speed report": the speed tour table and the title profile, `make -C gba profile-title`) and fix what it ranks first, the title included; then G8 tile backend for the island hub and menus, G9 combat HUD and the 30 fps lock with a test, then whatever `tools/perf_profile.sh` and `tools/ram_ledger.sh` rank first; measure on the ROM, not the host. **V10 landed 2026-11-07 (owner asked for 30 fps, logic untouched): a draw-side frame-time governor draws fewer far sprites on the invaded map only (not pixel-exact; new invaded `vhash-seq` f87e5596); libmgba: 17.5 -> 29.2 fps, 49.5 -> 30.7 ms, 30.1 logic ticks/s, every other scene identical; unmeasured on hardware (`docs/INVADED_SPEED.md`)**; **V11 landed 2026-11-07: body renders sliced (max 51 -> 40 ms), ground scan memo (exact, -1 ms on overworld maps), invaded 29.5 fps, hahaland 30.0**. **V12 landed 2026-11-07 (HUD panels on the invaded map, exact): while a toast overlaps the objective pill or the clock they are drawn plainly instead of captured (the capture was spoiled every frame and retried on the 2 / 4 / 8 frame cycle: the 9-10 tick bump), and the objective star's bob is the pill's `plat_panel_hole` strip, out of the key (no recapture four times in 40 ticks); host panel cache on the invaded toast scenario: 962 -> 180 panels drawn, 788 -> 5 spoiled, 4,040 frame hashes identical to the tree before; unmeasured on the GBA, check the invaded `vhash-seq` stays f87e5596 (`docs/INVADED_SPEED_V12.md`)**. **V13 landed 2026-11-07 (exact): `build_billboards` no longer redoes the per-prop work each drawn frame: a prop's cell, sprite, billboard flags and frame count are set once in `ownpc_load_props` (the floor height and the animation frame are still read live, so a lift under a prop still moves it); frame hashes of all 135 debug-beat / visit runs (day and night) identical to the tree before; expected gain is small (a few tenths of a ms), unmeasured on the GBA (`docs/INVADED_SPEED_V13.md`)**. **Music Studio measured 2026-11-07 (MS-P0, GBA half; `make -C gba profile-studio`, CI step "Profile the Music Studio", `docs/STUDIO.md` "GBA speed")**: SONG 35-36 ms / 23-24 fps, NOTES 33-35 ms / 25-29 fps, ORDER 31 ms / 30 fps, **LYRICS 60 ms / 12.4 fps (logic 22.7 ticks/s), CHOIR 52 ms / 15 fps, PERFORM 56 ms / 12.5-13 fps (logic 22.6)**; PERFORM is chibi-bound (`vid_blit` + `res_get_look` + `cache_get` about 35%), LYRICS is per-frame song derivation (`song_syllables`, `part_mask`, `song_singer_id` about 28%); fix order MS-P4, MS-P3, MS-P2, MS-P1, MS-P5 (`docs/MUSIC_STUDIO_ROADMAP.md`); PS1 not measured. **MS-P3 step 1 landed 2026-11-07 (exact): LYRICS 60.4 -> 45.3 ms, 12.4 -> 20.2 fps, logic 30.1 ticks/s; CHOIR and PERFORM unchanged (next: MS-P4 chibis).** **Then (exact): LYRICS 38.6 ms / 20.3 fps / 30.0 ticks/s (glyph runs, shot byte-identical); CHOIR 50.8, PERFORM 52.1 ms (look-hash memo `blh` 32 direct-mapped -> 40 fully associative).** **nov07d (MS-P2 step 1, exact): the studio backdrop is not drawn under the content panel, chibi look-hash slot hint: SONG 33.1, NOTES 29.6, ORDER 27.1, LYRICS 36.5, CHOIR 46.9, PERFORM 51.0 ms; read `docs/MUSIC_STUDIO_ROADMAP.md` "Facts to know before MS-P4" before the chibi tile cache.** **Owner goal: every screen at 30 drawn fps someday** (about 30 ms of work a frame at 30 ticks/s). CI tour 2026-11-06, work avg ms (what must come off to reach 30): island town hall menu 107.9 (78), island hub 97.4 (67), combat 85.7 / 83.1 (56 / 53), pet 67.6 (38), concert 62.3 (32), invaded 52.0 (22), story map 36.8 (7), hahaland 33.6 (4); already near it, spikes only: title 28.4 (max 63), overworld pause 23.0 (max 80), map walk 27.4. At 30 now: hellvile, park, house, options. `make -C gba profile-all` (and the CI step "Profile every scene") ranks every scene's hottest functions in one run: read it first, then fix the biggest gap that has a clear top function; the title's top functions are `vid_blit` 15%, `vid_rect_narrow` 11%, the text dilate pair 12%. **First `profile-all` (CI 2026-11-06), the top functions behind each gap:** island hub / town hall menu (97 / 108 ms): `fp_island_draw_map` 10%, `put` 9-11%, `vid_rect_narrow` / `vid_shade_words` 9%, `vm_span` 5%, `gba_udivmod` 4% (the G8 tile backend); combat (85 ms): `fx_dist` 15% (a 64-bit square root per call: many call sites only compare it with a radius, and `fx_dist(d) < r` is exactly `dx*dx+dy*dy < r*r`, so those need no root; count the calls per site on the host first), `put_row` 10%, `ba_face` 6%, `canvas_row` 5%; pet (66 ms): `lcd_rect` 17%, `fx_isqrt` 14%, `vid_rect` 11%; concert (61 ms): `vid_blit` 24%, `plat_checker_m4` 8%, `res_get_look` 7.5%; invaded (53 ms) and hahaland (34 ms): `ow_spr_rows` 30% / 36% (already a tight ARM loop, one table lookup a pixel: only fewer sprite pixels or hardware OBJ sprites (V7, changes pixels, needs a decision) go further), `ow_tile16` 7-11%, `plat_overworld` 11-14%; title: `vid_blit` 15%, `vid_rect_narrow` 11%, the text dilate pair 12%. **Music Studio PERFORM (nov07h, part 1 of `docs/STUDIO_PERFORM_FIXES.md`):** F3 karaoke text runs, F5 note-head rects (GBA) and F6 voice-tuning memo are in, pixel-identical (sung phase 57.7 -> 56.1 ms median, worst tick 69.4 -> 64.7 ms, logic 25.5 -> 26.5 ticks/s on the harness); part 2 (nov07i: F2 heart rects, F4 spot, glow/scallop ellipses, hidden LEDs, place once) took the sung phase to 47.0 ms median, logic 29.9 ticks/s on the harness; F1 is not possible exactly (shade is not retained); next MS-P4. **nov07p (exact): the 50.8 ms sung-phase tick was the first draw of a new line (`studio_part_of_raw`, ~100 `part_mask` calls, not a chibi render); one pass now: worst tick 50.8 -> 41.6 ms, 0 ticks over 50.2 ms, median still 37.6 ms (20 fps), `vhash-seq` identical, `profile-sung` Error 127 fixed (`\\` in the recipe), `docs/STUDIO_30FPS_PLAN.md` section 10.** **nov07q / nov07r (Option C, the choir as hardware sprites, exact by design, unmeasured on the GBA): the core planner (`choir_obj_plan`) and the GBA OBJ side (`gba/src/obj.c`, not yet built: CI is the first build) are in; the native test `studio_stage_sprites_equal_the_bitmap` (sprites on vs off, 700 ticks, 4 layouts, 2 sizes) found and fixed two planner gaps (follow-spot beam width, a singer's own note) and a wrong-planner test proves it catches them; 12.6 of 25 singers are sprites on risers (semicircle: 0); `make -C core unit` 877 passed; to check on CI: ROM builds with no IWRAM overflow, `vhash-seq` over the sung window stays `bee877b2`, the `studio_sung.txt` median (expect about 34-35 ms), `docs/STUDIO_30FPS_PLAN.md` section 11.**
5. **Customization:** P0 rest (the 96 -> 76 B repack and roster v2 if the p0 patch above is not in your tree, M0b age gate, saves v2), the other 12 M2 list appends, art review, M3, then P2-P6 (`docs/design/customization/P0_M0A_STATUS.md`).
6. **Dreamcast:** DC6-DC9 (DC3, DC4 and DC5 are done; the Flycast core is built and in the toolchain prefix). **DC5b, still open from DC5:** the music engine's IRQ tier on the Dreamcast (`CAP_DRV_IRQ` caps, `mus_drv_tick()` at 240 Hz inside `dcsnd_push()`, a `plat_voice` / `DoomoVoice` renderer with buses and effects, the tape calls), after MU2 / MU3 and S1's mute / duck / tape stop in IRQ mode, with a reference render to check it against (docs/DREAMCAST.md section 6, "The music engine's tier"). **Needs your call:** the sound's latency is about 100 ms (70-135) from the end of a tick to the speaker; going lower means KallistiOS at 1,000 Hz or our own AICA loop (section 6, "Latency"). **For DC6:** in `dc/tests/audio/combat.txt` the enemies' noises come at other times on the Dreamcast than natively for the same script (the combat logic diverges while the 3D view is a stub), so that comparison clips its envelopes at the 97th percentile.
7. **Script and languages:** W9-W12, W15 (approved, unwritten), W16; L8 re-translation of the rewritten text; pt-BR, fr, de, it, nl, pl untranslated; Chinese and Japanese need a font path (L10).
8. **Housekeeping:** README "Status" still describes a developer launcher; the saved branches (G3, G9, G11, H7, L4, W6, W7, ...) are not in the repo; mature pack art; console music backends; Ninth Cloud map; Feast scene (S186).

### What can be done next without the owner (pick by the standing focus: PS1 first unless GBA is named)

- GBA, invaded speed: the sprite-table cache and `ow_sprite` setup patches above, the spike hunt; each measured with `make -C gba profile-invaded` / `test-speed` in a session that installs the ARM toolchain and libmgba.
- CI: an apt retry + timeout patch for `ci.yml` (`Acquire::Retries`, `Acquire::http::Timeout`, a 10-minute cap on the install steps) so a mirror stall fails fast (not built yet).
- PS1: budgets, harness, RAM overlay plan (`docs/PS1.md`, `docs/PS1_LIFESIM.md`); needs the owner's CI logs, no PS1 toolchain in a chat.
- Life sim: part 3 (New life from the title), MP1 quests 2-4, Rook's and Wick's quests, polish items.
- Housekeeping: README "Status" text; customization list appends.

### Not playable yet

Dreamcast (2D only, not run on Flycast yet); deep customization (M2 painters, M3: no creator row or save field); mature pack art (PS1 has no stills); local simultaneous multiplayer; the S-HOUSE story house unlock; hand-written disaster scripts; pt-BR, fr, de, it, nl, pl text (engine only); Chinese / Japanese (no font path). GBA runs but is slow (invaded 14.9 fps, island hub ~7.5, town hall 6.5).

### Needs the owner

S-HOUSE definition; tone check of the disaster scripts; Flycast approval; any run on real GBA / PS1 hardware or PCSX-Redux; which pending patch is applied (a repo zip carries no git history).

### How patches work now (so a new session costs no time)

The repo carries its own patch tools. A session builds a zip with `tools/mkpatch.sh`; you apply it with **one short command** (see "Patch workflow" below and
`docs/NEXT_SESSION_PROMPT.md`). To start a session: upload the repo zip (nothing else is needed), say what you want, and the session reads
`docs/NEXT_SESSION_PROMPT.md`, this section and `docs/STATUS.md` first.

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
  U4 (2026-11-02, held-frame backdrop): the story overworld's pause menu calls `plat_hold_frame(1)` when it opens and releases it
  on every close (`PAUSE_OFF()`; `scene_set` releases too); while held, the 3D world is not redrawn, only the shade + panel.
  Platforms that answer 0 (the default) redraw as before. The native runner implements it (`plat_native.c`) and
  `story_overworld_walks_and_collides` checks it. GBA/PS1/DC still need their own `plat_hold_frame` (not built here); other
  menus (island START, town hall) still redraw their backdrop. `make -C core test` 706/706.
  PA6 (2026-11-02, New life): `playas_new_life_ok()` (story finished, not already a New life) and `playas_new_life(id)` (sets the
  New life flag = header byte 47 bit 2, then switches to a villager or family member, never the hero). The character switcher
  (`SCENE_PLAYAS`) shows a "Start a new life..." row once the story is finished: confirm, pick who, confirm again. The story stays
  finished, the hero stays a resident you can switch back to. Still open: a title-screen entry and a fresh-world New life (new
  island) are not built; this keeps the finished island. `make -C core test` 707/707.
  W8 closed in the core (2026-11-02, `make -C core test` 708/708). F2 (family UI) was already built in `fp_family.c`: dashboard,
  add-member wizard (neighbour / new person / pet / birth), care-all with the weakest-need pass, the Remembered tab, play-as and
  remove; the roadmap list was stale. LS5: `life_ending_facts` now also fills `partner_adult` (both adults, LS0 gate) and
  `happiness` (mean HAPPY of the living); story scripts get `CB_LIFE_FAMILY_ADD` (ARG = resident, VAR0 = 0 or `FAM_ERR_*`),
  `CB_LIFE_HOUSEHOLD`, `CB_LIFE_KIDS`, `CB_LIFE_PARTNER`. PA7: the ACTOR sites now use `playas_slot()` (grief hit in `sim_soul.c`,
  cooking and exorcism bonds, the edge-eviction bonus in `town.c`, the "not yourself" test in `story_talk.c`). Not built: the
  combat/ending art ties beyond those facts, F2 on GBA/PS1 screens (not run here).
  PA4: plot content now offers itself only to the hero (auto story beats, plot talk, side spots, nest fights).
- Script: style guide + locks (P0), Chapters 1-4 + epilogue rewritten (W1-W4),
  the 49 old quests rewritten (W5).
- House: golden coverage (H1), themed doors/windows + Towers 7B / Hell Pits rooms (H4),
  garden/plot (H5), animations (H6), lighting (H8), back facings (H9), 57 furniture
  pieces redrawn + roofs, GBA EWRAM for HOME (H10). PS1 disc links again (H0).
- House H3 (PS1 room shells, `ps1/src/room.c`): built and measured earlier (docs/PS1.md section 7; H4 took the Hell-Cellar from 17 to 30 fps).
  The H3 PS1 room shells are done; this line replaces the old "H3 open" entries (2026-11-02 reconcile). Not re-run since LS1.
- PA2 (hero choice cards): finished with the tagline fit, a header-byte test and the creator script fix (patch `doomo-pa2-hero-cards-nov02d`).
- PS1 journey meter: `ps1/harness/run.py` plays the native journey files and fails a journey over its `budgets.tsv` presses;
  the game `harness` target runs J6 + J7 (patch `doomo-ps1-journeys-nov02j`; not yet run on PCSX-Redux).
- U0 settings core (rebuilt 2026-11-02, patch `doomo-u0-settings-core-nov02o`): options with ids 64+ (`OPT_MATURE`, `OPT_LOSS`, `OPT_RUMBLE`) in
  `core/src/options_store.c`, the OPT2 save chunk, the 48 B machine config record (kept when `PLATF_CONFIG`), a real `opt_mature()`, Rumble row
  gates `plat_rumble`. `make -C core test` 701/701 + journeys OK. Still to do: GBA/PS1 builds + the GBA fps test (never built here), native config file.
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
  once NG+ can actually start a second run (Wave 7c: the title's New story on a finished save now offers New Game+).
- Life sim F4 part 1 (life cycle): pool members grow BABY 4 / TODDLER 6 / CHILD 10 / TEEN 10 town days into ADULT (`family_age_daily()` from
  `day_rollover`, progress in the low 5 bits of `FamilyMember.flags`), residents mirror the sim's stage, pets and adults do not age.
- Life sim F4 part 2 (births + toast): one "grew up!" toast per member that changes stage; `family_birth()` makes a BABY pool member
  with the CHILD role (needs the player's partner and room under the cap; `FAM_ERR_NOPARTNER`). Still to do in F4: family events,
  the birth/growing-up UI (F2's wizard), old age (F6). F4 parts 1 and 2, F6 and the life stage forms were written without compiling or testing: build and run
  `make -C core test` before relying on them.
- Life sim F6 (loss), core part: pool members can be lost: neglect (`family_care()` resets the count, a warning at 4 days, lost at 7),
  old age (a pool ELDER passes after 31 days in the household), `family_lose(id, cause)`, a memorial of the newest 18 losses
  (`family_memorial*()`), the LIFE chunk's LOSS section, `family_loss_set()` for the future "Loss" option (U0). A child is never lost
  (R5): minors only ever "move away". Still to do: nothing here since Wave 7 (memorial screen, resident deaths and pets' needs landed there).
- Life stage forms (F4): `look_for_stage()` (`core/src/portrait_age.c`): baby to elder body and face proportions on every portrait,
  body sprite and fighter image (docs/PORTRAIT.md "Life stages"). Still to do: baby outfits, a baby body shape, elder stoop and
  wrinkles, and a look on real hardware.
- Script E6a/E6b (started): PG-21 adult beats in Ch1-2 added so far: Hattie's "private shows" bit (`ch1/ep14.dsc`) and Vex's
  invoice line (`ch2/ep21.dsc`). More adult-only beats for Ch1-2 remain; run `make -C core story-check` after adding them.
- Life sim wave 1: shared registry/contracts incl. 15 endings, security code,
  family, pockets and profiles (K0), life stages + two-way adult-only romance
  gate (LS0), journey/tap meter (M0), PG-21 register + lint (E5).

- E8 (2026-11-05, GBA mature ROM variant, `make -C core test` 792/792, `tools/mpack/test_mpack.py` clean): `make -C gba MATURE_PACK=pack.dmpk` builds `build/doomogachi_mature.gba`: the game plus a DMPK pack for port gba linked into the ROM (`gba/src/packrom.c`: `.incbin` + strong `plat_pack_blob`; stock ROM links none of it). The pack is validated first (`mpack.py validate --port gba --gba-default-cap`, 4 MB) and a changed pack rebuilds `packrom.o`. The blob is addressable, so stills (RAW) are drawn in place from the cartridge and scripts register without a copy: 0 B of RAM, no `plat_mslot_buf`. Core: `PACK_CG_W_THIS/H_THIS` (pack.h; GBA 240x160) replace the hard-coded 320x240 in `mslot.c`. mpack: port gba now requires RAW 240x160 stills; `mpack.py example --port gba`; 5 new checks. Not built with the ARM toolchain or run in mgba here (none installed; `packrom.c` assembled and syntax-checked on the host, blob size matches the pack). Still to do: a mgba run of the variant with a real pack, the Redux run for E9.
- E9 (2026-11-05, PS1 mature disc, `make -C core test` 792/792, story-check clean): a streamed (disc) pack now plays. `pack_streamed()` / `pack_script_release()`, `plat_mslot_script_buf()` (plat_ext.h) and `PACK_SCRIPT_CAP_PS1` (8 KB, enforced by pack.c and mpack.py): the mslot scene reads the slot's script into the port's buffer when it starts and unbinds it when it leaves. PS1: `ps1/src/packdisc.c` reads PACK.DMP through a one-sector window; `cmake -DDOOMO_PACK_FILE=pack.dmpk` puts it on the disc and defines `DOOMO_PACK_ON_DISC` (the stock disc links none of it, +10 KB BSS only with a pack). Not built with the PS1 toolchain or run on PCSX-Redux here (syntax-checked against stub headers). Still to do: CG stills on PS1 (the display lists share RAM with the save scratch, so no 75 KB buffer yet: scripts play without art), a Redux run with a ps1 pack.
- E10 (2026-11-05, Dreamcast mature disc, `make -C core test` 792/792, `tools/mpack/test_mpack.py` clean, `sh dc/tests/packdisc_host.sh` passes): `dc/src/packdisc.c` is the DC twin of E9. `plat_pack_read` reads `/cd/PACK.DMP` through KOS `fs_*` with a 32 KB window (big requests go straight into the caller's buffer); `plat_mslot_script_buf` (32 KB, new `PACK_SCRIPT_CAP_DC`, enforced by pack.c and mpack.py) and `plat_mslot_buf` (a 320x240 still). `make -C dc PACK=pack.dmpk disc` validates the pack for port dc, defines `DOOMO_PACK_ON_DISC`, rebuilds packdisc.o when the pack changes and `mkcdi.sh` puts it on the disc root as PACK.DMP; the stock disc links none of it (+about 140 KB BSS only with a pack). Host test: the real packdisc.c + pack.c over a mocked KOS file API read an `mpack.py example --port dc` pack (open, port, CRC, still, script bind and release, window keeps disc reads under 40). Not built with KallistiOS or run in Flycast here (no toolchain); the still is decoded but not drawn until DC3's `plat_image` lands. Still to do: a Flycast run with a dc pack.
- Wave 7 (2026-11-04, F5 pets + F6 loss, `make -C core test` 761/761): pets are real household members with their own needs. `family_pet_kind/_kind_name` (six kinds from the look seed:
  imp-cat, hellhound pup, bat, slug, crow, bone hamster), `family_pet_fed` (100 down to 10 over the six untended days) and `family_pet_hungry` (from day 2) are derived from the
  neglect count, so nothing new is saved; `family_pet_tend(id, FAMPET_FEED/PLAY)` is care plus a mood lift. The family dashboard draws each pet's own 12x10 sprite, shows
  "Imp-cat - Hungry!" and gives a pet Feed and Play (no role change, no play-as: uplift is still later). F6: `family_resident_gone(id)` takes a household resident out when the
  sim kills or evicts them (`sim_lay_to_rest` calls it; `family_loss_daily` sweeps too): a memorial (old age / laid to rest = AGE, monster or demon = DEMON, neglect = NEGLECT,
  else ILLNESS), the partner's grief and a toast; a minor resident only "moves away" (R5); ghosts, the possessed and the soul-lost stay. The Remembered page now opens a memorial
  with A to leave Flowers (up to 3 posies, `family_memorial_flowers/_leave_flowers`, kept in the memorial record's first spare byte so old saves read 0). 4 new tests.
  Not built: a separate graveyard scene for the household (the Remembered page is the memorial screen), pet old age, the pets' uplift, GBA/PS1 screens (not run here).

- Wave 7c (2026-11-04, S-HOUSE New Game+, `make -C core test` 764/764): the New Game+ restart exists, so household tier 2 is reachable. `story_ng_plus_ready()` (finished story + the mode unlocked),
  `story_ng_plus_arm()` reads the run count and the endings seen/counts from the loaded finished story; the next `story_new_game()` applies them once: `SF_NGPLUS`, the run count (2 bits of
  `DoomoStry.ngplus`, up to 3) and the endings (`ENDF_NGPLUS_KEPT`), while the story itself starts over and the unlocks outlive it (the second finish grants tier 2: cap 20, GBA 10). The title:
  New story on a finished save asks "New Game+ / Brand new story / Never mind" (loads the save first); other saves behave as before (journeys unchanged). Not built: the "story house unlock"
  has no definition in the plans or the code (the story already opens your home through S2), so it still needs a decision; NG+ content (new lines, harder fights); GBA/PS1 screens (not run here).

- Wave 7b (2026-11-04, F4 family events, `make -C core test` 763/763): growing up is a family event. `milestone_` (family.c): each stage change adds a news line (first steps, started school,
  a teenager, all grown up, a wise elder), the one who grew up gets +4 mood and every other non-pet member +2; `family_birth` throws a welcome party (everyone else +3, a news line).
  `family_stage_days_left(id)` feeds the dashboard: a growing child's row reads "Child - Teen in 4d". The add-member wizard's baby path and the growing-up toast were already there.
  F3 (house occupancy) was already in `home_draw.c`. Not built: baby outfits, a baby body shape, elder stooping, GBA/PS1 screens (not run here).

- Wave 6 (2026-11-03, `make -C core test` 754/754 + story-check): PA9 `playas_romance_ok` is now the real play-as gate (anything touching
  the hero, the avatar or a family pool member needs both sides adult/elder, both directions accepting, never family, never a kid-coded
  premade; `family_add` partner check uses it). E4 core: `ending_route_*` (meter 40 offers, 120 dates; set/forward-only; adult + PA9 gate),
  story callbacks `CB_ROUTE_STAGE/ADD/SET/PARTNER`, and the finale now resolves secret > sexy (E12 Grumbleguts, E13 Vex, E14 any adult
  partner; only while Mature is not Off) > bad (E08 two or more neglect losses, E06 empty parade: alone, no friend/couple, happiness 1..34) > good.
  LS-QA part: rent collects itself after the island hub has idled 90 ticks (`life_rent` 4 -> 0 presses; Town Hall row still works).
  LS-IN (GBA): the VBlank IRQ latches held keys (`gba_key_latch`) and `read_pad` takes them, so a tap between two slow hub frames is no
  longer lost. WRITTEN WITHOUT AN ARM COMPILER: build the ROM and check the hub on hardware/mGBA before trusting it. E6d: two Ch4 punch-ups
  in `ch4/ep47.dsc` (text only, flow lock unchanged). U3b was already done. NOT done in step 1 (see step 2 below): LS-QA's "B = next problem" (life_help_next 9 -> 2)
  Step 2 (754 -> 757 tests): LS-QA done: B on the island hub opens the next citizen with a problem (`life_help_next` 9 -> 2 presses) and
  Help! buys a missing wanted item on the spot when you can afford it (`life_missing_item` 26 -> 2). E4 done in C: `ending_route_daily()`
  (day_rollover, only while Mature is not Off) grows a route by how close you are to the partner (friend +3, best friend +6, dating +10,
  married +14), moves it DATING -> KISSED -> STAYED -> COMMITTED (committed makes them the household partner) and queues the route's mature
  slot (mslots 8..16); the finale queues slots 2..4 for E12..E14; the story overworld shows a queued slot when idle (`ending_take_mslot`).
  Slots play as fade-to-black unless Mature is "On + pack". No dsc route scripts were needed. E6d: three more epilogue punch-ups
  (text only). Still open: the mature pack content itself (E2 packs), and a real-hardware look at the hub's B shortcut and the press latch.

**Started, saved on branches, not landed** (resume these first). CHECKED 2026-11-02: `git branch -a` shows only `claude/nice-allen-gr6czm`, so the branches named here (fut-u0, fut-g3, fut-g9, fut-g11, fut-h2, fut-h7, fut-l4, fut-w6, fut-w7, ...) are NOT in the repo; treat every item below as not started unless you can recover the branch. Hold the Door itself (waves, guests, panel, captions) is already built in `core/src/home_defend.c` / `home_draw.c`; what H7 still needs is only the PS1 black frame, which needs a PS1 run
- Life sim wave 2: LS1, U0, E0, PA0, E6a, E6b (may land before the session ends).
- G3, G9, H2, W6 (branches fut-g3 / fut-g9 / fut-h2 / fut-w6).
- G11, L4, H7, W7 (branches fut-g11 / fut-l4 / fut-h7 / fut-w7).

**Blocked on the user**
- Approve building the Flycast emulator core (`make -C dc/harness flycast`);
  until then the Dreamcast build is never actually booted in tests.
- ~~W15 (Bonejangles origin thread) needs a yes/no.~~ Answered 2026-11-04: yes, see the W15 decision record at the end of section 4.

## By effort: smallest first

Sizes: **S** = small (hours), **M** = medium (about a day), **L** = large (a few days),
**XL** = extra large (a week or more). Within each size, do the top lines first;
a package can only start once the ones it depends on (see the sections below) have landed.

### Tiny: finish what's already built (saved on branches)
1. U0 settings core (branch `fut-u0`, not in the repo, see section 0): fix one GBA fps test failure, then land.
2. Re-run the PS1 harness after LS1.
3. Approve and build the Flycast core (`make -C dc/harness flycast`), boot DC2.
4. Recover or redo the saved branches: G3, G9, G11, H7, L4, W6, W7 (not in the repo, see section 0).

### S: small
- Life sim: PA0 follow-ups, U1g (GBA config sector),
  E11 (mature filter + 18+ confirm), LS-IN (press latch), PA9 (romance/mature
  gates for heroes), PA3b (Spanish
  gender fixes), PA8 (done 2026-11-02: house, cuts, fishing, almanac, pause menu, care bonds and rewards follow the avatar via `playas_slot()`; GBA/PS1 not rebuilt), E10 (done 2026-11-05: Dreamcast pack
  adapter), E12 (endings integration), Z1 (joint gate).
- Script: W15 (Bonejangles origin; approved 2026-11-04, decisions recorded below, scripts not written yet).
- Languages: L9 (language picker).
- Dreamcast extras: X0 (contracts), X3 (VGA/display options).

### M: medium
- Life sim: E7 (content-pack format), U3a (pause shell), U5 (first-boot language
  picker), PA3a (gender-aware text), PA5 (character switcher), PA2 (done 2026-11-02; hero choice:
  SUNNY / MARVIN / your own), U6 (done 2026-11-02: U6a-U6e; Continue-first title and flow), E6c (Ch3
  punch-up), LS-QA (fewer taps), E6d (Ch4 + epilogue punch-up), LS6 (streamlined
  progression), E8 (GBA mature ROM), E9 (PS1 mature disc),
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
  PA4 (overworld handoff), LS4 (event and family content), U8 (GBA 3D Faster/Better done properly), S-HOUSE (story house unlock + tiers), MP1 (multiplayer + co-op heroes).
- GBA speed: G8 (tile backend: island hub and menus at 30 fps).
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
| W2 (done in the core 2026-11-02; GBA/PS1 builds and the PS1 harness re-run still to do) | ~~LS1~~, ~~U0~~, ~~E0~~, ~~PA0~~, ~~E6a~~, ~~E6b~~ | news as records + LIFE save chunk; settings core (OPT2); ending/pack contracts + VM ops; play-as contract; PG-21 punch-up of Ch1-2 |
| W3 (E7 done 2026-11-02) | PA1, U1g, ~~U1p~~, U2, E2, ~~E7~~ | play-as core (PLAY chunk, story barrier, actor retargeting); GBA config sector; PS1 config file; **options screen redesign**; mature scene slots + host; content-pack format + `mpack.py` + templates |
| W4 (done in the core 2026-11-02) | ~~F1~~, ~~E1~~, ~~U3a~~, ~~U5~~, ~~PA3a~~, ~~PA5~~ | **family core** (household up to 20 / GBA 10, Sims-style chosen relationships); ending engine; **pause shell**; first-boot language picker; gender-aware text; character switcher |
| W5 (done in the core 2026-11-03; GBA/PS1 builds and the PS1 harness re-run still to do) | ~~LS3~~, ~~PA2~~, ~~U6~~, ~~E11~~, ~~E3~~, ~~E6c~~ | life event engine; **hero choice: SUNNY (female), MARVIN (male fox), make-your-own**; Continue-first title + flow; mature filter + 18+ confirm; good/secret endings; Ch3 punch-up |
| W6 | LS-QA, LS-IN, PA9, U3b, E4, E6d | fewer taps; press latch; romance/mature gates for heroes; story pause; bad + sexy endings and romance routes; Ch4 + epilogue punch-up |
| W7 | PA4, U3c, LS4, LS6, E8, E9 | overworld handoff; island START menu; event + family content; streamlined progression; GBA mature ROM variant; PS1 mature disc |
| W8 (done in the core 2026-11-02) | ~~F2~~, ~~LS5~~, ~~PA6~~, ~~PA7~~, ~~PA8~~, ~~U4~~ | family dashboard + add-member wizard + care-all; story/combat/endings ties; **post-game "new life" as a villager**; play-as content; house/activities follow the avatar; held-frame backdrop |
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

H2 (GBA room shells at pixel parity; clears 33 expected-fail frames), H3 (done, PS1
room shells), H7 (Hold the Door panel, guests, captions, PS1 black frame),
H11's code part is done (the GBA keeps and saves the house state; the real-house GBA run, `make -C gba test-home`, passed 2026-11-02 with F3 in the tree: parity frames and fps gates held, `home_defend_w3` 22.9 ticks/s).

## 4. Priority 4: script

- Side quests: W6-W8 (side_a), W9-W11 (side_b), W12 free-play chatter,
  W15 (approved, see the decision record below), W16 integration.
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

One zip per patch in `~/storage/downloads/`, built by `tools/mkpatch.sh NAME "message" PRISTINE EDITED OUT` and unpacking to a same-named folder with
`apply.sh`, `patch.diff`, `commit.txt` (the commit message) and `RESTORE.txt` (the copyable commands). Use a NEW name for every patch.

**The command to send the owner (once `tools/dpatch.sh` is in the repo, which the `doomo-roadmap-formula-nov06` patch does):**

    sh ~/DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark/tools/dpatch.sh            # newest doomo-*.zip in Downloads
    sh ~/DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark/tools/dpatch.sh NAME       # a named one

It pulls (fast-forward only), refuses a dirty tree, unpacks, dry-runs, applies, commits with `commit.txt`, pushes, and logs to `~/dpatch.log`. Run twice is fine.
Undo before pushing: `git reset --hard HEAD~1`. **The long form** (needed only for the first patch, before `dpatch.sh` exists in the repo; replace NAME and MESSAGE;
~/vs is cleared by the next patch's `rm -rf ~/vs`):

    cd ~/DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark && git pull --ff-only && rm -rf ~/vs && mkdir ~/vs && unzip -qo ~/storage/downloads/NAME.zip -d ~/vs && sh ~/vs/NAME/apply.sh && git add -A && { git diff --cached --quiet || git commit -F ~/vs/NAME/commit.txt ; } && git push

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
| PS1 | `doomo-ps1-h3-reconcile-nov02k` | `docs: reconcile H3 (PS1 room shells) as done` | docs only: H3 leaves the open lists |
| docs | `doomo-roadmap-done-nov02l` | `docs: roadmap records PA2, U6, PS1 journeys, H3 as done` | docs only (marker: roadmap-done-nov02l) |
| PS1 | `doomo-ps1-journeys-all-nov02m` | `PS1: harness plays 15 core journeys (item, problem, rel, money, lang)` | PS1 focus step 2: mailbox + run.py learn the missing journey commands; not yet run on PCSX-Redux |
| U0 | `doomo-u0-settings-core-nov02o` | `U0: settings core (OPT_MATURE/LOSS/RUMBLE, OPT2 chunk, machine config record)` | rebuilt: the old fut-u0 branch is not in the repo; core + tests only, not yet built for GBA/PS1 |
| docs | `doomo-roadmap-branches-nov02n` | `docs: roadmap notes that the saved branches are not in the repo` | docs only |
| H7 | `doomo-h7-defend-frame-nov02p` | `H7: test that Hold the Door wave 3 draws a real frame` | the panel, guests and captions were already in home_defend.c / home_draw.c; adds `home_defend_scene_draws_a_frame` (not black). The PS1 black-frame check on PCSX-Redux is still to run |
| F3 | `doomo-f3-house-occupancy-nov02q` | `F3: household members stand in your own house (Live mode)` | up to 3 members of the household stand on free floor tiles (never the avatar you play); core only, not yet measured on GBA EWRAM / PS1 |
| docs | `doomo-roadmap-gba-house-nov02r` | `docs: roadmap records the real-house GBA run as passed` | docs only; the PS1 black-frame run on PCSX-Redux is still to do |
| E6a/E6b | `doomo-e6ab-ch12-punchup-nov02s` | `E6a/E6b: four more adult-only beats in Ch1-2 (Grampy, Haze, Sandwichy, Father Fuzz)` | text only: ep14, ep21, ep22, ep24; `make -C core test` passes, register lint 0 findings, save/flow locks ok; Spanish and the other languages are missing for L8 |
| U5 | `doomo-u5-lang-picker-nov02t` | `U5: first-boot language picker (SCENE_LANG_PICK)` | on a console with a config record and none saved yet, boot goes to the picker once: UP/DOWN applies the language live, A/START writes the record and opens the title. Native, tests and harnesses skip it. `make -C core test` 710/710. Not built for GBA/PS1 here; the Mature question and the security code (SCENE_SECURITY_CODE) are the other U5 parts and are still open |
| U3a | `doomo-u3a-pause-shell-nov02u` | `U3a: pause shell Music row (J4 11 -> 4 presses)` | every pause menu (combat, Hell Breach levels, arena, minigames) gets a Music row under Resume that LEFT/RIGHT changes on the spot (`pause_music_label/nudge` in fp_options.c); J4 is START, DOWN, LEFT, B, budget 4. Other rows moved down one: combat, pause_options and levels_modes tests updated. `make -C core test` 711/711, 154 scripts, 19 journeys. Not built for GBA/PS1 here |
| E1 | `doomo-w4-adult-gate-nov02v` | `E1: real adult allow-list (mature_adult_ok)` | strong mature_adult_ok: premade adult, name matches, not custom, not kid cast (compiled copy checked against kid_cast.txt); core only |
| E7 | `doomo-e7-mpack-nov02z` | `E7: tools/mpack/mpack.py (build, validate, info, SFW example) + tests` | host tool only, no RAM cost on any port: reads every constant from pack.h, LZ = the save codec, checks crc, caps, uids, payload bounds, script images (TXF_RAW, user pack ids); `make -C core story-check` runs `tools/mpack/test_mpack.py` (36 checks, incl. the C structs compiled against the built example); `make -C core test` 728/728. pack.c (E2) landed the same day, see its row |
| E2 (part 1) | `doomo-e2-pack-reader-nov02zb` | `E2: pack.c, the content pack reader (open, art, scripts) + tests` | `core/src/pack.c` overrides the weak pack_* defaults: validates like mpack.py (header, port, caps, CRC, every entry), serves RAW art in place or LZ art decoded into the caller's buffer, registers the scripts with `txt_user_set`; adds `pack_open_image / pack_close / pack_script_bind` to pack.h. Scripts are stored RAW (mpack.py enforces it). Nothing calls it yet, so the PS1 / GBA linkers drop it: 0 B resident. Still to do: the mslot host (`push mslot N`), `mslots.txt`. `make -C core test` 730/730; not built for GBA/PS1 here |
| RAM | `doomo-ps1-ram-nov02y` | `PS1: 12,768 B back (display lists 53 KB, shape cache pool 2,560, loading text 40)` | the game disc had overflowed the 64 KB stack reserve by 10,608 B |
| U5 | `doomo-w4-mature-code-nov02w` | `U5: first-boot Mature question and security code` | first-boot Mature question + SCENE_SECURITY_CODE (4-digit code, salted hash in config bytes 32..47); skipping the code keeps Mature Off; core only, not built for GBA/PS1 |
| F9 | `doomo-f9-away-helping-nov02x` | `F9: household members go away helping for a day (Phase 4)` | adult, elder or teen with bond 5+ leaves for a day and comes home the next town day with H$ or a small find; Family list shows "Away helping today"; nothing saved; not wired to LS3 (engine not in the tree); `make -C core test` 723/723, core only, not built for GBA/PS1 |
| E11 | `doomo-e11-options-mature-nov02y` | `E11: Mature row on the options screen (code check + 18+ confirm)` | Game page, last row: raising Mature asks the grown-ups' code when one is set, then "Are you 18 or older?"; lowering is free; Reset page puts it back to Off; three tests; `make -C core test` 726/726; core only, not built for GBA/PS1; the rest of the redesigned options screen is still U2 |
| E3 | `doomo-e3-secret-endings-nov02z` | `E3: secret finales E10 (every sparkle + D2) and E11 (NG+ with bottle and suitcase)` | ending_resolve_finale ranks secret first; ending_check_sparkles fires on the last sparkle; two tests; `make -C core test` 728/728; core only; nothing sets SF_NGPLUS yet so E11 waits for the NG+ restart; the good-ending (E02) rule is still provisional |
| E3 (final) | `doomo-w5-ls3-e3-e6c-nov03a` | `W5: LS3 life event engine, E3 final good-ending rule, E6c Ch3 punch-up` | `core/src/life_events.c` (10 events: potluck, picnic, surprise party, anniversary, lucky find, fan letter, rainy day, rotten day, talent show, stargazing; weight + cooldown + household condition; one try a day at 30 %, no household = no RNG draw, a STORY island waits for the cleared story like F9); saved as LIFE_SEC_EVENTS (2 + 10 B, omitted until one fires); `push`-free script hook `CB_LIFE_EVENT` (ARG = LEV_* id, VAR0 = 1 if it ran); `family_mood_add` / `family_name` made public. E3: E02 (the better good finale) now needs 3+ members, no neglect death AND one close tie (a housemate with a closest housemate, a child, a matched couple or island happiness 60+), else E01. E6c: six adult-only beats in Ch3 (`s31_wake`, `s31_squeaker`, `s31_rivet`, `s33_rivet_call`, `s35_hattie`, `s35_grampy`; none in a script with a kid; flow.lock re-written for those six); 9 new tests (`test_life_events.c`); `make -C core test` 747/747, register lint clean; core only, not built for GBA/PS1; Spanish and the other languages missing for L8 |
| CI | `doomo-ci-fix-gen-ps1-nov02za` | `CI: regenerate core/gen text packs; PS1 hcmd.c without atoi/strtoul` | check-gen was STALE on 110 generated files (text_*.c, lang_tab_es.c, txt_dict.c): regenerated with `make -C core gen`, `make -C core check-gen` now clean; PS1 build failed (-Werror, implicit atoi/strtoul in ps1/src/hcmd.c): replaced by local h_atoi/h_num (decimal + 0x hex); core + ps1 only |
| CI | `doomo-ci-ps1-diag-nov02zb` | `CI: PS1 link says how far over the RAM limit it is and which objects stay resident` | the PS1 disc fails at link: image + bss end at 0x801f3b00, 15,104 B past the 64 KB stack reserve (limit 0x801f0000); no size change here: elf2exe prints the overshoot and ovl_plan runs with OVL_WHY=1 so the next log lists the biggest resident objects; the real cut is the next patch |
| CI | `doomo-ci-ps1-modal-cb-nov02zc` | `CI: PS1 overlay planner knows the family wizard and Mature-row modal callbacks` | ovl_plan.py MODAL_CALLBACKS lacked fp_family's 12 ui_choice/ui_confirm/ui_text_entry callbacks, fp_options cb_adult (E11), fp_apartments house_pick and playas_scene nl_done/nl_start, so they and what they reach stayed in resident RAM (fp_family 5.8 KB, fp_options 4.5 KB in the log); PS1 disc was 15,104 B past the stack reserve; expected to free up to ~10 KB, not measured here (no PS1 toolchain); planner still keeps any callback resident that a resident taker reaches |
| CI | `doomo-ci-ps1-slot-diag-nov02zd` | `CI: PS1 log shows which overlay group sets the slot and the image/bss/slot layout` | after nov02zc the PS1 disc is 10,608 B over the stack reserve (was 15,104 B); no size change here: ovl_plan prints each group's RAM need (house net of its borrowed .bss) and elf2exe prints image / bss / slot ends, so the next cut targets what actually binds |
| U2 | `doomo-u2-options-redesign-nov02zf` | `U2: options screen redesign (Quick page, Loss + Rumble rows, presets)` | new first page Quick (Language, Music, Sound, Text speed, Difficulty, Gore, Preset); pages shift by one (OPTPAGE_QUICK = 0, callers passing 0 land on it); Loss row (Life Sim) and Rumble row (Controls, only with PLATF_RUMBLE); Preset row: Default, Comfort, Classic DOOM, Cozy life, Hardcore, Max accessibility (never touches language, volumes, Mature, Monster Mode, cheats, skins, Performance page); J3 9 -> 6 presses (target 5 not met: J5 needs Language as the first row); cb_preset added to ovl_plan.py; Spanish text for the Quick page, presets and new rows added later (`doomo-u2-spanish-nov04`, test `lang_options_u2_spanish`, 765/765); core only, not built for GBA/PS1 |

How the owner applies patches (so nobody has to ask again): from Termux, one pasted line per patch, oldest first, each line
is the full template above (`cd` repo, `git pull --ff-only`, `rm -rf ~/vs && mkdir ~/vs`, `unzip -qo` the zip from
`~/storage/downloads/` into `~/vs`, `sh ~/vs/NAME/apply.sh`, `git add -A`, `git commit -m`, `git push`). No cleanup step at the end.
Rules for whoever builds a patch: a NEW zip name every time (`doomo-<id>-<what>-nov02<letter>`; the letter keeps counting up),
the zip holds one folder of the same name with `apply.sh` in it, `apply.sh` runs from the repo root, is safe to run twice
(guard with `grep -q` or `git apply --reverse --check`), needs no network and no python (sed, grep, cat, git apply only),
and the reply gives the line with NAME and MESSAGE filled in, plus a ROADMAP row (the apply.sh adds it) for each patch.

<!-- roadmap-branches-nov02n -->

<!-- doomo-u0-settings-core-nov02o -->
<!-- doomo-h7-defend-frame-nov02p -->
<!-- doomo-f3-house-occupancy-nov02q -->
<!-- doomo-roadmap-gba-house-nov02r -->
<!-- doomo-e6ab-ch12-punchup-nov02s -->
<!-- doomo-u5-lang-picker-nov02t -->
<!-- doomo-u3a-pause-shell-nov02u -->
<!-- doomo-w4-adult-gate-nov02v -->
<!-- doomo-w4-mature-code-nov02w -->
<!-- doomo-f9-away-helping-nov02x -->
<!-- doomo-e11-options-mature-nov02y -->
<!-- doomo-e3-secret-endings-nov02z -->
<!-- doomo-ci-fix-gen-ps1-nov02za -->
<!-- doomo-ci-ps1-diag-nov02zb -->
<!-- doomo-ci-ps1-modal-cb-nov02zc -->
<!-- doomo-ci-ps1-slot-diag-nov02zd -->
<!-- doomo-u2-options-redesign-nov02zf -->

<!-- doomo-ls4-life-events-nov04a -->
LS4 (2026-11-04, `doomo-ls4-life-events-nov04a`): six more household life events appended to the LS3 table (bedtime story, homework help, an elder's old stories, a pet's new trick, movie night, bake-off); append-only, old saves load (missing cooldown bytes read 0); news lines fit NEWS_LEN 64; 3 new tests, `make -C core test` 768/768, story-check clean; core only, not built for GBA/PS1 here.

<!-- doomo-pa10-stress-save-nov04b -->
PA10 part 1 (2026-11-04, `doomo-pa10-stress-save-nov04b`): the play-as stress save. `pa10_stress_save_round_trips_under_the_gate` fills tier 2's 20-member household (4 pets), starts a New life as a family member, loses one, saves: 10,086 B against the 32,768 B gate, then loads and checks the avatar, the New life flag, the story barrier and the pet count. `make -C core test` 769/769. Still open in PA10: the ledgers, and integration with E12/Z1.
PA10 part 2 (2026-11-05, `doomo-pa10-pockets-nov05k`): per-avatar pockets. Money, the bag and health follow the avatar: a switch saves the outgoing avatar's H$, health, armour and bag into a pocket and loads the incoming one's (the live town fields stay where every shop and gift site reads them, so no call site changed). The hero's whole 64-kind bag is a two-slot stash held while someone else is played; 14 villager pockets of 16 stacks, newest first; no pocket means a seeded purse H$40..199 and two snacks. The PLAY chunk grows to at most 800 B (header bytes 22 / 23); an old 32 B chunk loads and loses nothing; a hostile chunk is clamped. Six new tests in test_playas.c, stress save 10,123 B against 32,768 B. docs/PLAYAS.md section 9. Not measured on GBA EWRAM / PS1 (about +0.7 KB .bss). Not built: screens that show other avatars' pockets, per-avatar weapons and ammo.
PA10 part 3 (2026-11-05, `doomo-pa10-newlife-nov05l`): New life from the title and in a fresh world. With a finished story save, Story Mode > New story now asks "What now?": New life here (loads the finished world, opens the "who are you?" list, B keeps the hero), New life, fresh world (a new Classic free play island that starts as a villager; the saved island stays until saved over, with a warning), New Game+ (if unlocked), Brand new story, Never mind. `playas_new_life_arm()` lets a fresh free play world start a New life (never saved, ends with the world); `SCENE_PLAYAS` takes `PLAYAS_ARG_PICK_NEWLIFE`. Three new tests in test_playas.c. docs/PLAYAS.md section 10. Not built: a fresh world from the Free Play menu without a finished story, a title script (no runner command sets a finished story), GBA/PS1 builds (not run here).
PA10 part 3 finish (2026-11-05, `doomo-pa10-finish-nov05m`): the open items of part 3. L / R on the New Island screen (Classic or Random card) start the island as a villager without a finished story: the "who are you?" list opens over the new island. After any New life pick a narrator line gives the first day (whose, and the H$ in the pocket). Three title / island scripts (`pa10_title_new_life`, `pa10_title_fresh_world`, `pa10_new_island_villager`) play it with the screens, using a new runner command `story-done`. `make -C core test`: unit 848, scripts 160 runs OK. docs/PS1_LIFESIM.md gets rows for pockets and New life (Native only; nothing built for GBA/PS1 or run on a console).
PA10 part 4 (2026-11-05, `doomo-pa10-picker-nov05n`): the profile picker screen for MP0. A "Players..." row in the Play as switcher (or `PLAYAS_ARG_PROFILES`) lists the hero and up to 3 guests, A plays as one, "Add a player..." picks a free neighbour (named after them), SELECT moves a guest out. Core only, no new scene. 3 new tests in test_playas.c, `make -C core test` 851/851, story-check clean. docs/PLAYAS.md section 11. Not built: typed guest names, FREE gates, MP1 co-op heroes, the S-HOUSE decision, GBA/PS1 builds.
PA10 part 4b (2026-11-05, `doomo-pa10-names-free-nov05o`): typed guest names (the picker opens the name box, blank falls back to the villager's) and the FREE gate: Free play opens play-as only after an ending has been reached (`playas_free_unlocked()`, persists across worlds; fresh-world New life, mid-switch saves and saves with guests stay open). New runner command `ending-seen`; fixtures updated. `make -C core test` 853/853, scripts 160 OK. docs/PLAYAS.md section 11. Not built: renaming a guest later, MP1, the S-HOUSE decision, GBA/PS1 builds.
MP1 part 1 (2026-11-05, `doomo-mp1-coop-rename-nov05p`): renaming a guest (START on their row, `prof_rename`) and co-op heroes: Ember, Rook and Pip fill `HERO_COOP_FIRST..+2` (placeholder names and seeds in `hero.c`); a co-op guest plays a housemate made from the hero's look seed (takes a household place, moves out with them). The picker lists the free co-op heroes first. `make -C core test` 857/857, scripts 160 OK, story-check clean. docs/PLAYAS.md (MP1 part 1). Not built: co-op hero story content and abilities, local simultaneous multiplayer, helping in another world, GBA/PS1 builds.

<!-- doomo-z1-joint-gate-nov04c -->
Z1 (2026-11-04, `doomo-z1-joint-gate-nov04c`): the joint gate across play-as, family, life events and endings, as one test (`z1_joint_gate_across_the_four_areas`): a tier-2 household of 20, two endings seen, three life events fired; the tour plays as every playable member and checks the story flag, beat and `story_fingerprint` never change; then one save (10,192 B, gate 32,768 B) loads back with the same avatar, household, endings, event total and story fingerprint. `make -C core test` 770/770. Not covered: GBA/PS1 builds (the gate's other half, run on those ports).

<!-- doomo-e12-ending-daily-nov04d -->
E12 (part, 2026-11-04, `doomo-e12-ending-daily-nov04d`): endings integration. E04 "Family Portrait" and E09 "Bonejangles Bows" were only checked at the finale / when the Feast ends, so a partner, an ETERNAL house room or the machine heart that came later never counted. New `ending_daily()` (called from sim `day_rollover` after `ending_route_daily`) re-runs both checks (story mode waits for the cleared story; free play and NG+ do not), and picking up the machine heart checks E09 at once. 2 new tests, `make -C core test` 772/772. E09's trigger is no longer provisional; still open for E12: the mature-pack side (E2 packs) and a real-hardware look.

<!-- doomo-e2-slot-tryout-nov04e -->
E2 try-out (2026-11-04, `doomo-e2-slot-tryout-nov04e`): `pack.c` and `mslot.c` were already built; this runs them hard. `e2_every_slot_falls_back_safely_without_a_pack` pushes every slot 0..63, an out-of-range one and 255 with Mature "On + pack" and no pack (and slot 2 with Mature Off): each finishes, hands the previous scene back, leaves no fade and reports 0. `e2_the_example_pack_plays_only_the_slots_it_has` opens the SFW example pack: slot 0 plays (1), every slot it lacks falls back (0), and closing the pack returns slot 0 to the fade. `make -C core test` 774/774. Not done: loading the real `hellvile_after_dark.dmpk` (it lives outside the repo) and a real-hardware look.

<!-- doomo-w15-decisions-nov04f -->
### W15 decision record (2026-11-04, the owner's answers; docs only, no script written yet)

- **Go:** Bonejangles' origin thread goes into the game (W15 is unblocked).
- **The failed exorcist is Father Fuzz.** It is a reveal near the late-middle of the game, which we place in
  **Ch3**. It has to fit Lady Smog (eleven years ago) staying his established failed rite and wound.
- **The chain's in-world name is "Boneyard Buffet & Bandstand"** (the "Jangles' Jamboree Family Fun Feedery"
  proposal is dropped). Never the real work's names, characters or places.
- **Still open for W15:** the tapes' and the fans' names (working names "the Bandstand Tapes" and "bandstanders",
  to confirm); the exact Ch3 episode and the beat for the reveal; whether Father Fuzz's own scripts
  (`sa_fuzz` and the Ch1-2 lines) get one foreshadowing line before it.
- **Rules that hold:** the origin is a joke Bonejangles tells and everyone else finds horrifying, never a mystery the player
  solves, no ARG mechanic, no ending gated on lore (SCRIPT_STYLE 1.3). No line names the exorcist before the reveal (foreshadowing
  is allowed). "Clack!" stays (SCRIPT_LOCKS: 6 uses; W15 adds the animatronic layer around it).
- **When W15 is written:** `make -C core story-check` (flow lock regenerated with a reason), `make -C core gen && make -C core check-gen`,
  `make -C core test`, and the Spanish and other languages need the L8 pass.

<!-- doomo-f10-story-danger-nov04g -->
F10 story hooks (2026-11-04, `doomo-f10-story-danger-nov04g`): a hand-written story beat can put a household member in danger. `family_story_danger(kind, id)` starts a disaster flagged as a story danger (never the avatar): same warning and one-day chance to care for them, but no dice (a minor always pulls through; an adult, elder or pet nobody cared for is lost through `family_lose()`), and with the Loss option Off it plays as a rescue (news "was rescued in time!", nobody lost) instead of being dropped. `family_disaster_rescue(id)` lets a scene save them early; `family_disaster_is_story()` reads the flag. Script hooks in the CB_FAM block (`story_int.h`): `CB_FAM_DANGER` (ARG bits 0..3 = FAMDIS_*, bits 4..19 = member id), `CB_FAM_RESCUE`, `CB_FAM_DANGER_KIND`, `CB_FAM_DANGER_TARGET`. Nothing saved (a load cancels it). 2 new tests (`family_story_danger_rules`, `family_story_danger_callbacks`), `make -C core test` 776/776, story-check clean; core only, not built for GBA/PS1 here. Still open for F10: the actual hand-written scripts that call these hooks (they need the owner's tone check), and the owner's tone read of the disaster lines.

<!-- doomo-mp0-profiles-nov04h -->
MP0 (2026-11-04, `doomo-mp0-profiles-nov04h`): player profiles in one shared world save, Animal Crossing style. `core/src/profiles.c` + `doomo/profiles.h`: up to 3 guests move in beside the hero (`prof_guest_add(name, villager, hero_kind)`: 1..11 printable chars, an alive villager who is not the hero or another guest's, co-op hero kinds only once MP1 fills one; needs play-as to be available, so a story save waits for the cleared story). `prof_switch(p)` takes the pad as a profile through `playas_switch`; the active profile is derived from the avatar, so nothing extra is saved and a load always agrees. Each guest keeps 128 progress flags, 16 quest bytes, up to 12 ties to villagers, a room id and 4 letters (`prof_flag/quest/rel/room/mail_*`; profile 0 is the save itself, so those are no-ops there). `playas_profile / _avatar / _hero` are now real (weak defaults in weak_playas.c stay). 'PROF' chunk layout 1: 32 B header + 96 B per guest (320 B for 3, contract ceiling 2,336 B); empty chunk with no guests, so a save without guests only grows by one 10 B chunk header (test_save bound widened by one SAVE_BUDGET_CHUNK_HDR). Header byte 47 bit 3 (`PLAYAS_HF_PROFILES`) is set while guests live here. `prof_check_daily()` (sim day_rollover, after playas_check) moves out a guest whose villager died or left, with a toast; a removed guest who was active sends you back to the hero. 7 new tests (`test_profiles.c`: rules, switching, story lock, state, chunk round trip + hostile bytes, full save round trip, daily check), `make -C core test` 784/784, story-check clean; core only, not built for GBA/PS1 here (about 320 B of statics + ~1.5 KB code resident everywhere: check the PS1 RAM overlay plan and GBA EWRAM before trusting it). Not built: the profile picker scene, guests' own houses (a room only), money/items/health (the PLAY pockets, PA10), co-op hero content and local multiplayer (MP1).

<!-- doomo-ls4b-helpers-events-nov04i -->
LS4b (2026-11-04, `doomo-ls4b-helpers-events-nov04i`): the F9 household helpers are wired into the LS3 life event engine. Two events appended to the table (saved cooldowns are by index, old saves read 0): `LEV_HELPING_HAND` (a housemate with bond 5+ brings home H$, a gift or a smile, through `family_help_do`) and `LEV_AWAY_TRIP` (an adult, elder or teen with bond 5+ heads out and comes home the next town day, through `family_away_start`; one trip at a time). Helpers: `family_helper_count(away)`, `family_helper_pick(away)`, `family_help_call(id)`. Script hook `CB_LIFE_HELP` (ARG = member id: they head out or help at once; VAR0 = 1 if they did). The F9 daily pass (`family_help_daily`) still runs on its own roll; the events add the table's rolls, cooldowns and the script hook. 2 life-event section bytes more (LEV_COUNT 16 -> 18). 2 new tests (`life_events_helping_hand_and_away_trip`, `family_help_callback`), `make -C core test` 786/786; core only, not built for GBA/PS1 here. Still open: the disaster story scripts and the owner's tone read of the helper lines.

<!-- doomo-ls6-new-island-nov04j -->
LS6 (2026-11-04, `doomo-ls6-new-island-nov04j`): streamlined New Island flow. On the New Island screen A now builds the island at once (the "Would you like to give your island a name?" question is gone: it cost a press to open and a press to skip its typewriter), and START asks for a name first (`Name your island` entry, then builds; backing out of the entry stays on the card; the hint line shows "START Name" on the Classic and Empty cards; the Town Hall rename row still works later). Journeys: `life_new_island` 5 -> 3 presses (38 ticks), `j9_new_island` 9 -> 7 presses (86 ticks); budgets and the GAME_DESIGN press table lowered. The press-count preludes of the other journeys, the free-play scripts and the GBA/PS1 harness scripts (`life_speed`, `speed_tour`, `perf_island`, `perf_life`, `prof_island`) shortened to match. 2 new tests (`freeplay_new_island_start_names_it_first`, `freeplay_new_island_naming_can_be_cancelled`), `make -C core test` 788/788; core only, the GBA/PS1 harness scripts were edited but not run here. Still open: the welcome box's own two presses (skip the typewriter, dismiss) are left as they are; the Random card's hint line has no room for "START Name" (START still works there).

<!-- doomo-ls7-toast-polish-nov04k -->
LS7 (2026-11-04, `doomo-ls7-toast-polish-nov04k`): presentation polish found by reviewing the life sim screens at 240x160. (1) Toasts: a line wider than its pill (the island's "A visits a place, START opens the menu." ran off the screen edge) now scrolls to its end and the toast holds for the scroll; when more toasts wait behind it a short one moves along after 48 ticks instead of 75, so a burst (NEW UNLOCK + hint) stops covering every screen's title row for six seconds. `ui_text_clip` is now public in ui.h (the clipped-run helper that `fp_marquee` already used moved there; same pixels). (2) The Family screen's title window ends above its tabs on the 240x160 layout (it overlapped them by 2 px). 1 new test (`toast_backlog_clears_and_long_lines_scroll`), `make -C core test` 789/789; core only, not built for GBA/PS1 here. Seen and left alone: the toast and the screens' title rows share the top strip (a fixed toast slot would need a per-screen margin), the Family member menu covers the list rows, and the new screens' text is English-only until the L8 pass.

<!-- doomo-ls8-name-scroll-nov04l -->
LS8 (2026-11-04, `doomo-ls8-name-scroll-nov04l`): the care screen's name plate clipped long names ("Judge Hoo"); a name wider than the plate now scrolls with `fp_marquee` like the mood and activity lines under it, and so does a non-living status line. `make -C core test` 789/789; core only.
| E10 | `doomo-e10-dc-pack-nov05a` | `E10: Dreamcast mature disc (PACK.DMP reader, make -C dc PACK=...)` | `dc/src/packdisc.c`, `PACK_SCRIPT_CAP_DC`, mkcdi.sh + Makefile `PACK=`, host test `dc/tests/packdisc_host.sh`; not built with KOS here |
| docs | `doomo-roadmap-status-nov05b` | `docs: status check (playable vs left) + docs/STATUS.md` | docs only: adds docs/STATUS.md and a "Status check (2026-11-05)" section to this file |

<!-- doomo-roadmap-status-nov05b -->
## Status check (2026-11-05)

Full write-up: [docs/STATUS.md](docs/STATUS.md). `make -C core test` ran clean on this tree (0 failures, 154 scripts, 19
journeys) and all 69 scenes link as real code. Not built or run for GBA, PS1 or Dreamcast in that check.
Playable on the core: Free Play, creator, life sim and family, Hell Breach shooter, 4-chapter story + epilogue, house, minigames.
Biggest open items: run the console builds and harnesses (PS1 harness re-run, GBA latch/hub checks), Dreamcast DC3-DC9,
GBA fps, mature pack art, MP0/MP1, customization P0-P6, console music, W9-W16 script, L8/L10 languages, and the README
status table (stale: still describes a developer launcher).
| P0.M0a (slice) | `doomo-p0-m0a-gene-nov05c` | `P0.M0a: gene codec + v1 bridge + look_pool/settings layouts` | additive only: `gene.h/.c` (83 fields, byte-wise get/set, v1 bridge), `look_pool.h`, `settings.h`, `test_gene.c` (7 tests; all 1000 premade + 4000 generated citizens round-trip). `make -C core test` 799/799, check-gen clean; the 96 -> 76 B repack, roster v2, look_build.py and the stubs are still open (docs/design/customization/P0_M0A_STATUS.md); not built for GBA/PS1 here |
| P0.M0a (age) | `doomo-p0-age-map-nov05d` | `P0.M0a: settle the age mapping (gene age = life stage) + family test` | supersedes nov05c (contains its files): `AGE_*` are aliases of `LIFE_*`, the bridge carries the stage, 2 tests (every stage; a 20-member household through 60 days of growing up). `make -C core test` 800/800; still additive, no save or screen changes; not built for GBA/PS1 here |
| P0.M0a (ids) | `doomo-p0-creator-ids-nov05g` | `P0.M0a: creator rows/presets by gene field id, save.c v1 wire struct` | creator rows and presets use gene field ids; `save.c` v1 wire struct. `make -C core test` clean |
| P0.M0a (copy-out) | `doomo-p0-whole-look-nov05h` | `P0.M0a part 2: whole-look/array sites copy out, no pointers into the record` | `res_get/set_look/voice/persona`, `RES_*_VIEW`; all 94 `RES_*_PTR` sites gone, `repack_sites.py report` = 0 host-build uses. 800/800, 154 scripts. Next: the 96 -> 76 flip (docs/design/customization/P0_M0A_STATUS.md, "Next, in order"); not built for GBA/PS1 here |
| P1.M2a | `doomo-p1m2a-update` | `P1.M2a: wave-2 face fields pupil, sclera, blush2 (painters + tests)` | `DF_PUPIL`, `DF_SCLERA`, `DF_BLUSH2`; `portrait_render_deep()` for tests and contact sheets |
| P1.M2b | `doomo-p1m2b-update` | `P1.M2b: wave-2 ear fold and glasses (DF_EAR_FOLD, DF_GLASSES2, painters + tests)` | 7 ear folds, 4 glasses styles; `make unit` 826/826 |
| P1.M2c | `doomo-p1m2c-update` | `P1.M2c: wave-2 hood head (DF_HOOD_HEAD, painter + tests)` | 3 hood-up styles, hoodie/cloak and no hat only; `make unit` 827/827 |
| P1.M2d | `doomo-p1m2d-update` | `P1.M2d: wave-2 face C-tier deep fields (lash, lid, liner, bags, freckle, mole, scar2)` | seven bust-only fields in `face_deep()`; `make unit` 828/828 |

| P1.M2e | `doomo-p1m2e-update` | `P1.M2e: B76 halos +6 (appended after the live count)` | Double Ring, Heart Halo, Square Halo, Neon Glitch, Flower Ring, Saturn Ring in `ph_halo()`; `HALO_COUNT` stays 6 (live), `HALO_ALL_COUNT` = 12 (render limit); `make unit` and `make test` 829/829 |

<!-- doomo-p1m2-status-nov05 -->
## Customization P1.M2 status (2026-11-05)

**Done:** all thirteen wave-2 face/head painters (pupil, sclera, blush2, glasses2, ear fold, hood head, lash, lid, liner,
bags, freckle, mole, scar2), each with a "0 is today, every choice distinct" test. The Classic golden is unchanged.
**Playable:** nothing new (the six new halos are not selectable in the creator yet). The painters read the in-RAM shim pool, which only tests fill; no creator row, save field or
Look Book entry sets a deep value, so in-game portraits are exactly as before. They show only in `portrait_render_deep()`
(tests, contact sheets).
**List appends (1 of 13 painted):** B76 halos +6 (P1.M2e). Appended after the live count, so `HALO_COUNT` (creator, saves,
sanitising, roster) is still 6 and `roster_uses_every_catalog_option` holds; the painter draws all 12 (`HALO_ALL_COUNT`). A list
flips live (`HALO_COUNT` = `HALO_ALL_COUNT`) only when a premade wears every new value (P6) and the art review passes.
**Left in M2:** the other 12 list appends: F01 head shapes +12, F07 eye styles +16, F25 eye count to 6, F29 brows +12,
F35 noses +12, F47 mouths +12, H01 hair +32, H13 facial hair +8, E01 ears +16, B66 horns +10, B79 auras +12, O25 hats +24;
plus the art review + contact sheets (chibi, back view, hood/hat/glasses interplay). **After M2:** M3 (`DF_MAKEUP`, `DF_FACEPAINT`, `DF_AURA2`,
A28 expressions, aura/halo detail), the real deep-field set from P0 (replaces `look_fields.h` / `look_pool.c`), `live_p1.txt`,
the mGBA bench, a PS1-toolchain build of the LZ blocks, then P2-P6. Host unit suite only; not built for GBA/PS1/DC here.
| lifesim1 | `doomo-lifesim1-pet-uplift-nov05i` | `lifesim 1: pets grow old, the late-game uplift (F5 part 2)` | `family_pet_old / _age_days / _old_days_left`, `family_uplift_check / family_uplift / family_uplifted_kind`, Uplift row in a pet's menu, story hooks `CB_FAM_UPLIFT`, `CB_FAM_UPLIFT_OK`; 11 new tests; core + PS1 planner list only |

<!-- doomo-lifesim1-pet-uplift-nov05i -->
Life sim core, part 1 (2026-11-05, `doomo-lifesim1-pet-uplift-nov05i`): F5 is finished. **Old age:** a pet that has lived 45 town days in the household is old (a toast and a news line);
its old days count in the low 5 bits of its flags, and with Loss on it passes peacefully after 31 of them (FAMLOSS_AGE, a memorial), with a "slowing down" toast four days before. Loss off: the count holds
5 short of the end, so the warning always plays first. An old save's long-lived pet starts its count at 0 on the first morning (a full grace month, never a death on load). **Uplift:** the late-game ritual
(unlock `UL_LIFE_PET_UPLIFT`, granted with household tier 1 when the story is finished, also by `family_tier_sync`; tier 1 alone is enough, so saves from before this get it too): a pet with bond 8+ becomes
a HOUSEMATE, ADULT, who talks, can be played as (`playas_can_be`), helps, joins free will and never grows old. It is the answer to a pet's old age. An uplifted pet is never a partner (the partner gate refuses it, and a pet
never was one). Nothing new is saved by the old age; the uplift is kept in the POOL identity's first spare byte (kind + 1; old saves read 0; `family_pool_load` accepts 0..FAMPET_COUNT there and still refuses a used second spare byte).
The family screen: a pet's menu gets an Uplift row once unlocked ("Uplift (closer bond)" until bond 8, then confirm); rows read "Uplift ready", "Old", "Uplifted Imp-cat". `make -C core test` 839/839, story-check clean, check-gen clean.
Not built: a look for the uplifted pet (they use the person look made from their seed), GBA/PS1 screens (not run here). Done since then: the profile picker, typed names, renaming, the Free mode gate and the co-op hero characters (PA10 part 4, 4b, MP1 part 1 below). What is still open is in "Left, life sim core".

<!-- doomo-roadmap-status-nov05j -->
| docs | `doomo-roadmap-status-nov05j` | `docs: status update (done, playable, left) after life sim part 1` | docs only: rewrites docs/STATUS.md |
| gba | `doomo-gba-flash-settle-nov06a` | `gba: flash verify waits out the program time (0x5F busy value, test-coresave)` | `gba/src/flash_ram.c`: `fl_settle()` (400 spins, ~2,500 cycles) before the verify pass of `fl_program`; mGBA answers every read of a sector being programmed with 0x5F, so a chunk whose last programmed byte is 0x5F passed `fl_wait` at once and the verify then read 0x5F for byte 0 (CI: `test-coresave` `save 1 failed`, `program/verify failed at 0xd0a0`, after the `town-fill` rewrite). Flash block 1,148 B of its 1,152 B stack copy. Checked: `make -C gba test` exit 0, `test-speed` runs |

## Status check (2026-11-05, second update)

Full write-up: [docs/STATUS.md](docs/STATUS.md). `make -C core test` 839/839, story-check and check-gen clean; not built or run for GBA, PS1 or Dreamcast.
**Done:** the life sim core through F5 (pets grow old, the uplift), F6-F10, LS3/LS4, tiers and NG+, play-as and New life, MP0 core, endings and routes; settings, title flow, content packs, script Ch1-4.
**Playable on the core:** Free Play, creator, life sim and family (now with pet old age and the uplift), Hell Breach, the 4-chapter story + epilogue, house, minigames.
**Left, life sim core (2026-11-05):** co-op hero content (story lines, quests, abilities for Ember/Rook/Pip, and their final names and looks; the ones in `hero.c` are placeholders), local simultaneous multiplayer and helping in another world (the rest of MP1), the S-HOUSE story house unlock (needs a definition from the owner), small polish (uplifted pet look, baby outfits and body shape, elder stoop, a household graveyard scene, the hand-written disaster scripts); then the console runs, Dreamcast, GBA speed, mature art, customization P0-P6, W9-W16, L8/L10.

MP1 part 2 (2026-11-05, `doomo-mp1-coop-content-nov05r`): co-op hero content. Final names and looks: Ember (imp), Rook (owlkin), Wick (mothkin; replaces the placeholder "Pip", the show's child). Story lines, four side quests and two abilities each in docs/COOP_HEROES.md, as data in `hero_coop_info()`. `make -C core test` 858 unit. Not built: the quest scenes, the ability effects in play, local simultaneous multiplayer, helping in another world, GBA/PS1 builds.

MP1 part 3 (2026-11-05, `doomo-mp1-ember-quest-nov05s`): Ember's Ch1 co-op quest "Wrong Side of the Mat" as a script (`coop_ember_1`, pack `zc_embr`), flow.lock updated (new pack zc_embr, script coop_ember_1, its strings: deliberate, the first co-op quest scene). `make -C core test`: unit 859, scripts 160 OK. Not built: the trigger, the saved done bit, the other 11 quests, abilities in play, local simultaneous multiplayer, GBA/PS1 builds.

MP1 part 4 (2026-11-05, `doomo-mp1-ember-trigger-nov05t`): the co-op quest trigger. A guest playing Ember on the overworld gets her Ch1 quest once (`story_coop_tick`), post-game; the played bit is a saved PROF guest flag (no save-lock change). `make -C core test`: unit 860, scripts 160 OK, journeys 19 OK. Not built: the reward, quests 2-4 on later days, Rook's and Wick's quests, abilities in play, local simultaneous multiplayer, GBA/PS1 builds.

**nov07s (Music Studio PERFORM, exact, measured on libmgba):** the choir sprites (Option C) were never used on CI (`stg: ok 97 emit 0 spr 0`; semicircle plan chose 0 of 25). Notes and mouths are now sprites too (`plat_stage_mini`), the crowd cover box and the spot beam are exact, so 16.5 of 25 singers are sprites on the semicircle: sung median 40.2 -> 36.7 ms, 233 -> 265 drawn ticks of 330. Still above 33.3: next the `bodyspr_image` look-change epoch (-1.8 ms) and the 8 bitmap singers; `docs/STUDIO_30FPS_PLAN.md` section 12. CI now prints the `stg:` line under the sung-phase step.
