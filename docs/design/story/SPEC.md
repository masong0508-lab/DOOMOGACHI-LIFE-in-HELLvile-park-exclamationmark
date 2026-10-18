# SPEC: Story, Dream House and Comic Timing (one implementation spec, GBA + PS1)

DOOMOGACHI LIFE in HELLvile Park! Version 2, 2026-10-18 (adversarial review applied). Status: an implementation spec. **§12 "Critique fixes" is binding and wins over every earlier section of this SPEC when they disagree.** Nothing in the repo was touched. The repo was read only, for spot checks.

**What the user asked for.** "Try ingraining in both games a very deep, heartfelt, scary but funny story with even more side missions and the ability to visit and decorate/build your dream house/room apartment (a good 2.5 on ps1, superbly cheap but effective isometric on gba)... From the video you produce to make sure you borrow that amount of spastic, elastic timing and comedic timing... while making it feel long too of course."

The "video" is the trailer in `trailer/`: `timeline.c` (15 scenes), `scenes/s01..s15*.c`, `tk.c` (the toolkit: `tk_bounce`, `tk_ease`), `tk_chibi.c` (the plush critter renderer and its idle periods) and `music.c` (whose `FR(n)` cue coupling is the thing we must not copy).

**This SPEC merges three designs into one buildable plan:**

| Source (same folder) | It is the source of truth for | This SPEC adds |
|---|---|---|
| `story_final.md` (256 KB) | What happens and why: §0-§13 main path, §14 the 109 side missions, §15 the engine, §16 the bytes, §17 the save, §18 the tests | the file owners, the order of work, the fixes in §2, and the acceptance gates |
| `housing_final.md` (88 KB) | How the house works: the rooms, the editor, the catalog, the score, visiting, the renderers, the `HOME` chunk and the house tests | the same, plus the story mapping from `story_final.md` §4 |
| `comic_timing.md` (75 KB) | The timing vocabulary: the beat ladder, DROP/POP/SLAM, springs, shake profiles, Appendices A-C and the `tween.h`/`beat.h`/`camfx.h` API | renames that avoid name clashes (§2) |

**Which document wins.** When two documents disagree, this SPEC's §12 decides first, then §2. Otherwise `story_final.md` decides *when* and *why*, and `housing_final.md` decides *how the house works*. Section numbers below like "SF §15.2" mean `story_final.md`, "HF §6.4" means `housing_final.md`, and "CT §7.2" means `comic_timing.md`.

**Coordination with other work.**
* **The customisation spec.** `custom/SPEC.md` does not exist yet. There are two candidates: `custom/design_packed.md` (design A: a 96-byte record, `LOOK`/`UNL2` chunks) and `custom/design_grow.md` (design B: 48-byte hot record + heap, settings in the save header, `unlocks2` in town). §6.3 lists every file that both this SPEC and either candidate touch, with a hand-off for each. The hand-offs work whichever design is chosen.
* **`integration_todo.md`.** Its open items that touch this work: the GBA EWRAM budget, PS1 RAM headroom, unlock id collisions, weak registries read through a volatile pointer, the GBA harness lacking `scene`/`push`, and story levels in level select. They are folded into packages H2, S3 and I9.
* **Agents editing the repo right now.** The GBA memory pass (task #77, EWRAM ≥ 24 KB free) owns its files until it merges (§6.3). That merge is gate G0.

---

## 0. Contents

1. What ships
2. Fixes to the source designs (decisions binding on every package)
3. Architecture
4. Contracts (written by S0, frozen after it merges)
5. The authoring format for story content (`.dsc`)
6. File ownership (one owner per file; shared files and hand-offs)
7. Work packages: S0, S1, S2, S3, S4, S5, H1, H2, H3, H4, I9
8. Order of work and merge plan
9. Global budgets and gates
10. Definition of done (the whole SPEC)
11. Risks
12. Critique fixes (binding; wins over §1-§11)
- Appendix A: which package owns each mission and episode
- Appendix B: which package owns each test and runner script
- Appendix C: checks every content author runs

---

## 1. What ships

| | Both platforms |
|---|---|
| **Main story** | A crayon prologue (P0), then 4 chapters with 27 episodes, 8 specials (4 flashbacks, 2 dreams, 2 interludes), an epilogue (E1-E4) and post-game with NG+. About **13 h** for the main path, **+12-16 h** of side content, **+3-8 h** post-game. Every beat is frame-timed (SF §7 timing contract). The player speaks exactly three lines (SF §8.4). |
| **Side missions** | 109 new missions, S100-S217, of which 9 ids are retired: 22 chains (84 missions) and 25 stand-alones, using 18 verbs. The 49 existing quests are re-timed. 74 missions reward furniture. Each mission has a "Later" callback. |
| **Comic timing engine** | Tween tables and springs, a frame-exact beat scheduler, camera effects (shake, punch, zoom, flash, iris, palette lerp), dialogue beat codes, cutscene stages, overworld NPC and camera scripting, a running-gag spawner, a wrongness table, four audio hooks, a 13-sound comedy SFX kit, and lint tests that enforce the timing. All of it is calibrated against the trailer's tables (CT Appendices A-C). |
| **Dream house** | The Pink Door Cottage with 6 rooms, plus Hillvile Towers 7B, a Hell Pits flat and the Park Plot (post-game build). Build / Live / Photo modes; a catalog of 306-330 pieces; four stats; grades COSY / LIT / HELD / HOME / ETERNAL; the Ch2 haunting; Hold the Door in Ch3; visits to any of the 1000 generated homes; gifts; wants; HomeMart and the Black Market. |
| **GBA** | 2:1 isometric in mode 4. A static bitmap with dirty rects. OBJ sprites for the cursor, the held piece, emotes and squash. BG2 affine, BLDY and palette DMA for camera effects. 0 B of new IWRAM. |
| **PS1** | A GTE diorama room with the near walls cut away. Furniture is built live from primitive recipes, small decor is billboards, and recolours are CLUT rows. A tilted narrow-FOV camera orbits on a spring. A partial static-vertex cache. The house is the new `home` overlay group. |

---

## 2. Fixes to the source designs (binding on every package)

Each item was checked against the tree. The repo was read, never written.

| # | Topic | The source said | This SPEC decides | Why |
|---|---|---|---|---|
| R1 | Scene scratch size on PS1 | HF §7.3: "PS1 `DOOMO_SCENE_SCRATCH` is 32 KB", so the 18 KB vertex cache goes there | `scene.h:52` is `12u * 1024u` on every platform. The PS1 vertex cache is **8 B per vertex** (`sxy` u32, `sz` u16, lit RGB565 u16) and lives in `DoomoRoomView.plat_cache`, which is the tail of the house's scene scratch: about **8 KB = 1,024 vertices**. The cache fills in this order: floor, walls, then pieces largest first. Vertices past the cap take the uncached path. The scratch is **not** enlarged. | Enlarging the PS1 scratch costs 20 KB of resident RAM and breaks the 128 KB headroom gate (task #79). |
| R2 | Where story text lives on PS1 | SF §16.5: a 36 KB pack window inside the story overlay group | The **whole pack window is resident** on the PS1: one episode slot (12 KB), one chapter-common slot (6 KB), two side slots (8 KB each, LRU) and a 2 KB directory, **36 KB of `.bss`**. The story group is then code only (about 67 KB). There is no re-fetch after a return from combat or exorcism. | The siege beat runs in the `home` group, Feast captions in the `cutscene` group, and boss captions in `combat`. All of them need story text while the story group is not loaded. The net resident change is about neutral, **≈ −1 KB** (§9). |
| R3 | Cutscenes are their own overlay group | SF §16.5 put the cutscene stage renderer in the story group | `ovl_plan.py` already has a `cutscene` group (`cutscene$|story_feast$|grumbleguts_gfx$`). The beat-driven stage renderer belongs to that group, with a cap of 100 KB. Story to cutscene stays one CD read, as today. The rule "no smash cut crosses a group" still holds, because every cutscene runs inside a single push. | Matches the tree. |
| R4 | Resident files must not match a group's regex | SF/HF named the resident files `story_vm.c` and `home_api.c` | The story group's regex `story(_\w+)?$` and the planned home regex `home(_\w+)?$` would pull those files into overlays. Resident files are therefore named **`scriptvm.c`**, **`txt.c`**, **`stry_save.c`**, **`house_api.c`**, `tween*.c`, `beat.c` and `camfx.c`. Overlay-only story code keeps the `story_` prefix. | `ovl_plan.py:61` |
| R5 | Name clash on `BEAT_*` | CT §7.2 defines `BEAT_TICK`, `BEAT_HOLD`, and so on as ladder constants | `story.h` already uses `BEAT_*` for story beats (`BEAT_C1_ARRIVE … BEAT_DONE, BEAT_COUNT`), and SF §17.1 appends 36 more after `BEAT_DONE`. The ladder constants become **`RUNG_R2, RUNG_R3, RUNG_R5, RUNG_R8, RUNG_R13, RUNG_R21, RUNG_R26`**, and the comic-timing tick constant becomes `RUNG_TICK`. Scheduler ops keep `BOP_*` and functions keep `beat_*`. | Avoids two meanings for one prefix. |
| R6 | Text id width | SF §16.2: `(pack << 10) \| index`, which allows 64 packs | The plan has 67 packs (35 episode, 5 chapter-common, 26 side, 1 Feast). A text id is a u16 of **`pack:7 | index:9`**: at most 128 packs of 512 strings each. `textpack.py` fails the build if a pack goes over. | 67 > 64. |
| R7 | Unlock ids | HF: action 564..595 for the house. SF: no allocation | **home 564..595** (32 ids) in a new `unlocks_home.c`. **Story additions go in the free story ids 677..735** (59 ids; 608..676 are used today: `USTORY_REWARD_FIRST = 660` plus 17 reward ids): 12 new cutscene ids at 677..688 (the old `USTORY_CUT_FIRST + CUT_*` run ends at 643, and CH1_DONE is at 644), 9 new songs at 689..697 (`SONGU` would collide at 630+), and 13 misc ids at 698..710 (3 title variants, 6 Breach variants, 2 arena modes, 2 creator parts). 711..735 are spare. **Action 596..607 stays unassigned, as a reserve.** **Foundation 202..239 is reserved for the customisation spec**, as both candidates plan. | 1024 ids, all in ranges. Keep the uniqueness test (task #72) green. |
| R8 | Trophies | SF names about 45 mission trophies ("Nobody Knows Who", ...) | Trophies are **not unlock ids**. Each one is a TROPHY/STORY catalog piece (an H1 row) and an entry in the quest log's Trophies list, read from the `STRY.side2` done bits (S2). The Library's Trophies wing keeps its unlock-based list unchanged. | Saves about 45 unlock ids. Trophies cost 0 B of save. |
| R9 | Catalog size | HF: 306 pieces, `FURN_MAX 320`, 52 mission rewards. SF: 74 furniture rewards | **`FURN_MAX` = 384** (ids stay 9 bits). The catalog targets 306-330 pieces; every SF furniture reward must exist (a test checks). `HOME.owned` grows to 192 B and `seen_new` to 48 B, so the `HOME` v2 worst case is **3,408 B** (SF's 3,368 + 40). | Room for the 22 rewards HF did not list. |
| R10 | `HOME` version | HF v1: 2,609 B for 6 rooms | **v2 only** (SF §17.3): the cottage's 6 rooms + Towers 7B + Hell Pits flat + Park Plot = 9 room records, a visitor log and `decorate_for` progress, **3,408 B worst case** (R9). The Park Plot is one 10x8 open-kind room where `FF_WALL` pieces may go on any grid edge (post-game Build). There is no v1 on disk to migrate, because the house has never shipped. | |
| R11 | Episode numbers in HF §10 | HF uses the heart bible's numbering | Use SF §4's mapping: HF "Ep 1.2 / 1.5 / 2.2 / 3.1 / 3.5 / 4.3 / 4.6" are SF Ep 1.2 / 1.5 / 2.3 / 3.1-3.2 / 3.7 / 4.4 / 4.6. Hold the Door wave 1 is **Ep 3.2**, under the red night sky (Ch3 has no daylight, so HF's "daylight wave" becomes a red-sky wave of 3 imps). The siege is **Ep 3.7**. | SF wins on *when*. |
| R12 | `plat_screen_fx` | HF §9.1 adds `plat_screen_fx(dx, dy, zoom, fade)` | **Dropped.** The house uses `camfx_*` (S1), which drives `plat_post_offset/scale/blend/iris`. Only one post-FX path exists. | No duplicate code on each platform. |
| R13 | Save slot 1 (free play, 90,112 B) | HF: free-play worst case + `HOME` = 61.8 KB. Custom A: ≈86,648 B. Custom B: computed bound 89,606 B, with the heap sized to the ceiling | Either custom design + `HOME` 3,408 B **exceeds 90,112 B in the static worst case**. This SPEC adds a **joint dynamic budget**: `save_budget.h` (S0) provides `save_budget_left(slot)`. The house editor (placement, photo, blueprint save) and the customisation heap commit (`res_commit()` / `save_measure()`) both refuse to grow past it, with a shared "save space" meter. In free play, the `HOME` chunk is 10 B when nobody has moved in. Custom B's lower bound on the heap (34,034 B, so v1 saves migrate) only applies to v1 saves, and v1 saves have no house, so the budgets do not collide. I9's test `save_joint_worst_case` proves ≤ 90,112 B for: a full heap with an empty house, a full house with the heap capped, and a random mix. | The bound is computed, not measured (custom risk #3). |
| R14 | `save_ext` registry | HF: "turn the switch into a 12-slot array" | The registry is weak extern pointers (`doomo_save_ext_studio/extra/freeplay/fishcook/test`). S0 adds **`doomo_save_ext_home`** and **`doomo_save_ext_story2`** (`STRY`) in the same style, read through a volatile pointer, and adds **`uint32_t max_size`** to `DoomoSaveExt` (custom B wants the same member; S0 adds it once, for everyone). | Follows the existing pattern. |
| R15 | Feast "outcome" cutscene already in the tree | SF S186 §5: the outcome is "cartoon, **non-sexual**, vanity and gluttony punished" and ends on the cheeks, a "well, that happened" look and the iris | `core/src/story_feast.c` (`CUT_FEAST_POP`, 21 panels) already exists. Panels 0-8 match S186. **Panels 9-19 go beyond the spec**: "...that's hot." and the mob, a candlelit afterglow, a slow look into the lens, the bike pump, and a "long night" with a queue and "the pump passed down the line". S5 **cuts panels 9-19** and ends the cutscene at SF's last beat (the cheeks drifting down, the culprit's look, `[IRIS○ out 10]`, the title card), then ports the rest onto `beat.h` stages. S5 keeps `grumbleguts_gfx.c` and its API. | The SF §3 hard rules and the S186 staging are canon: innuendo only, joke on his vanity and gluttony. |
| R16 | Trophies, songs and SFX registries | — | Songs are a `const AudioSong audio_songs[SONG_COUNT]` array of MML pointers in `songs.c`. S0 appends 9 `SONG_*` ids (`SONG_MINGLE, SONG_WALL_LULLABY, SONG_TWO_CHOIRS, SONG_BOAT_FLOATS, SONG_THIRTEEN_BELLS, SONG_HELL_LULLABY, SONG_DOUBLE_ACT, SONG_HEEHEE_JINGLE, SONG_NUMBER_TEN`) and 9 table rows that point at `extern const char doomo_mml_<name>_ch[AUDIO_CH][1]`. Weak silent placeholders live in `stubs.c`. Each song's owner writes a strong definition in its own `core/src/mml_<name>.c`. The SFX kit gets `SFX_*` ids appended by S0, with data by S1. | Disjoint files, no array edits after S0. |
| R17 | Scratch inside the overworld | SF puts the Feast's guest table in `scene_scratch` | During the Feast and the Say It walk, `SCENE_STORY` owns the scratch (`OWLD`). S1 adds a **512 B script extension area to `OwScratch`** (`ow_scratch_ext()`), and those scripted overworld scenes keep their state there (Feast guests: 64 × 4 B; Say It walk: ≤ 256 B). | One owner per scratch block. |
| R18 | GBA OAM shadow | HF: a 1 KB OAM shadow in `gba_big` while the house runs | S1's `gba/src/obj.c` keeps a **32-entry shadow (256 B of `.bss`)** for squash on hero actors in every scene. The house calls `obj_set_shadow(buf, 128)` with a buffer in its `gba_big` claim. OAM is copied by DMA from `video_flip()`, which is THUMB in ROM. **No IWRAM growth.** | |
| R19 | Furniture data split | HF: `furngen` reads one recipe file per piece | Stats and art live in separate files with separate owners: `core/data/furn/catalog.txt` (H1: name, desc, price, flags, score, footprint, theme, mass) and `core/data/furn/art/*.txt` (H4: prims, facings, animation). `core/tools/furngen.c` (H4) joins them by key and fails the build on any unmatched key. Decorate-for layouts live in `core/data/home/layouts_<owner>.txt`, one file per content package. | Disjoint ownership. |
| R20 | Runner commands on the platforms | `integration_todo.md`: "GBA harness lacks scene/push commands" | H2 adds `scene`, `push`, `seed`, `expect-flag` to `gba/harness/harness.c` + `gba/src/hcmd.c`. H3 adds them to `ps1/src/hcmd.c` + `ps1/harness/run.py`. S2 adds `seed`, `expect-text` and `expect-flag` to `core/native/runner_main.c`. | Needed for every platform script below. |
| R21 | Test registry cap | — | `core/tests/test_main.c` has `MAX_TESTS 512` and 337 tests today. About 110 are planned here, and the customisation work adds more. S0 raises the cap to **1024**. | Without this, tests are silently dropped. |
| R22 | Grumbleguts as a citizen | SF §15.11: a "resident shadow record" in the sim | `resident.h`, `sim_*.c` and `town.h` belong to the customisation spec, so **this SPEC adds no field to them**. Grumbleguts' friendship, romance state, dishes fed, `RES_DEAD` and the no-rite lock live in `STRY.grumble` plus the `SF_GRUMBLE_*` bits. The dating ticker, the graveyard and the Library read him through `story_grumble_*()` getters (S5). | No layout conflict with the customisation spec. |

---

## 3. Architecture

```
 content (.dsc text, per package)      house data (catalog.txt, art/*.txt, layouts_*.txt)
        |  core/tools/textpack.py (S2)              |  core/tools/furngen.c (H4)
        v                                           v
 core/gen/text_*.c + side_table.c          core/gen/furn_data.c        ps1: TEXT.BIN (S2)
 + possess_table.c                                  |
        |                                           |
 scriptvm.c (resident; S2) --CALL HOME--> house_api.c (resident; H1) --push--> SCENE_HOME (home group:
   | SAY / CHOICE / PUSH / NPC / CAM / BEAT / ...                               home*.c H1+H4)
   v                                                                              | plat_room()
 beat.c (resident; S1) --tw_* / camfx_* / audio_*-->  platform hooks (S1)  <------+
   |  stages: cutscene.c (S1)       overworld_npc.c / overworld.c (S1)     plat_room: gba/src/room.c (H2)
   v                                                                                  ps1/src/room.c (H3)
 ui_dialog.c (S1): beat codes, pill, portraits; text from txt.c (resident; S2)
 saves: STRY (stry_save.c, S2)  HOME (home.c, H1)  joint budget (save_budget.c, S0)
```

* **One clock.** Everything advances on the 30 Hz tick. Player input only moves anchors (`WAIT_A`). Every hold, gap and insert is an integer number of frames stored in ROM data (SF §18.4).
* **One RNG for the story.** `story_rng(ctx)` is an xorshift seeded from `(save hash ^ day ^ beat ^ ctx)` and lives in `scriptvm.c`. No wall clock is read anywhere.
* **No C callbacks escape into the dialogue queue.** `ui_say` copies the decoded line into its 200-character queue entry. This is what lets 73 KB of story code leave PS1 resident memory (SF §16.5).
* **No smash cut, insert, iris, dream or knock crosses an overlay group.** A cut into combat, exorcism, cooking, concert, creator, pet or home happens only at an episode boundary or a quest step (SF §16.5, HF §9.5).
* **The house is reached only through `scene_push(&SCENE_HOME, arg)` and `house_api.c`.** Story code never touches `doomo_furn[]`.

---

## 4. Contracts (package S0 writes these; frozen after S0 merges)

The contract headers are written once, by S0. After S0 merges, each header has a **maintainer**, who may only **add** declarations. Changing an existing declaration needs an RFC to I9 that every affected package acknowledges.

| Header | Maintainer after S0 | Contents |
|---|---|---|
| `core/include/doomo/tween.h` | S1 | CT §7.1 verbatim, plus `TW_POP_SOUL, TW_POP_BLOB, TW_POP_BURST, TW_SLAM_STICK2, TW_DROP_LET_SX/SY/DY, TW_HEART_LAND_SQ, TW_DEVICE_DY, TW_SIGN_DY, TW_WINDOW_BOING_DY, TW_BRAKE_DX, TW_ZOOM_WOBBLE_DY, TW_HOP2, TW_DEADPAN_DY` (SF §15.4) |
| `core/include/doomo/beat.h` | S1 | CT §7.2 with R5's renames, plus `BOP_STILL, BOP_TWITCH, BOP_NPC, BOP_CAM, BOP_TINT, BOP_PAL_LERP, BOP_NOBUTTON, BOP_SHOT, BOP_HEADTURN`. Label conventions: `L_PUNCH` and `L_BUTTON` are reserved label ids 30 and 31, which the lint reads. A `beat_set_actor_clock(actor, mode)` hook for platform draw code. |
| `core/include/doomo/camfx.h` | S1 | CT §7.3 (14 `SHK_*` profiles) plus `camfx_pal_lerp(pal_id, level16)` with `PAL_SEPIA, PAL_CRAYON, PAL_HELLIFY, PAL_LILAC` |
| `core/include/doomo/story_vm.h` | S2 | `StoryOp {u8 op, u8 a, u16 b}`, `StoryScript`, the op enum of SF §15.2 (`SAY, SAY_X, CHOICE, PLAYER_LINE, JIF, JIFN, JLT, JGE, SETF, CLRF, SETV, ADDV, BEAT, WAIT, HOLD, WAIT_A, PUSH, POSSESS, UNPOSSESS, GIVE, TAKE, REWARD, BEFRIEND, TELEPORT, CAST, NPC, CAM, SFX, MUSIC, MUTE, DUCK, TAPESTOP, RESUME_ON_BEAT, SHAKE, PUNCH, FLASH, FADE, IRIS, TINT, PAL_LERP, INSERT, CUT, FREEZE_CAP, HOME, QUEST, SET_BEAT, CALL, RET, RNG, LATER, DATA, END`), `PEND_*` push kinds (combat, exorcism, cutscene, pet, workshop, home, cooking, concert, creator, dream, timing, minigame_side), `HOME_*` sub-ops (`SET_PHASE, PLACE_GIFT, REMOVE_ONLY, GOAL, DECORATE_FOR, HAUNT_STEP, DEFEND, VISIT`), `vm_start/vm_tick/vm_resume(result)`, `story_rng`, `story_fingerprint`. `LATER` (new) schedules a callback script n minutes later. |
| `core/include/doomo/txt.h` | S2 | `txt_get(id, out, cap)`, `txt_pack_load(pack)`, `txt_pack_is_live(pack)`, `DlgLine {i16 who, u8 expr, u8 flags, u16 text}` with flags `DL_DEADPAN, DL_PUNCH, DL_PILL, DL_SHOUT, DL_HEART, DL_GURGLE`. Text id = `pack:7 | index:9` (R6). |
| `core/include/doomo/home.h`, `furn.h` | H1 | HF §8.2 `DoomoFurn` (with `FURN_MAX 384`, flags2 `FF_PORTAL, FF_CURSED, FF_NO_SQUASH`), `FCAT_*`, `FI_*`, `THEME_*`, `HomeStat`, grades (`HG_COSY, HG_LIT, HG_HELD, HG_HOME, HG_ETERNAL`), `home_phase` enum (`HP_COSY, HP_HAUNTED, HP_BARRICADED, HP_EMPTY, HP_OPEN, HP_DREAM`), home ids (`HOME_COTTAGE, HOME_TOWERS_7B, HOME_PITS_FLAT, HOME_PARK_PLOT`), the `SCENE_HOME` arg encoding (`-1` own home, `2+id` visit, `0x4000 | mode` story modes) |
| `core/include/doomo/house_api.h` | H1 | resident: `home_grade(room)`, `home_stat(stat)`, `home_grant(furn)`, `home_has(furn)`, `home_place_gift(furn)`, `home_remove_only(on)`, `home_goal_check(goal)`, `home_decorate_for(layout, tol)`, `home_haunt_step(night)`, `home_defend_result()`, `home_rivet_visits()`, `home_days_since_fire()`, `home_set_phase()`, `home_phase()` |
| `core/include/doomo/platform.h` (additions only) | S1 for post-FX, H1 for the room | `plat_image_xform(ax, ay, img, sx_q8, sy_q8, flags, color)`, `plat_post_offset/scale/blend/iris`, `plat_pal_lerp(pal_id, level16)`, HF §9.1 `DoomoRoomObj/Actor/View` + `plat_room()`; the **weak software defaults** live in `core/src/plat_weak_fx.c` (S0) |
| `core/include/doomo/audio.h` (additions) | S1 | CT §7.5 + SF §15.10: `audio_music_mute/duck/tapestop/resume_on_beat`, `audio_music_dual(a, b)`, `audio_sfx_pitch(sfx, semis)`; appended `SFX_SCRATCH, SFX_BOING, SFX_POP2, SFX_THUD, SFX_WHOOSH, SFX_KAZOO, SFX_BEEP, SFX_GLITCH, SFX_TIK, SFX_SHWIP, SFX_TINK, SFX_FANFARE, SFX_CHOIR, SFX_PLUCK, SFX_CREAK` and `VOICE_GURGLE`; appended `SONG_*` (R16) |
| `core/include/doomo/save_ext.h` (additions) | S0 → I9 | `max_size` member; `doomo_save_ext_home`, `doomo_save_ext_story2` (R14) |
| `core/include/doomo/save_budget.h` (new) | S0 → I9 | `save_budget_left(slot)`, `save_budget_meter(slot)` (0..100), `SAVE_SLOT_STORY_CAP 40960`, `SAVE_SLOT_FP_CAP 90112` (R13) |
| `core/include/doomo/story.h` (additions) | S2 | 36 `BEAT_*` ids appended after `BEAT_DONE` (P0, 27 episodes, 8 specials; old ids alias onto episodes, never reordered); new `CUT_*` ids appended after `CUT_FEAST_POP` (`CUT_P0, CUT_BOTTLE, CUT_F1..CUT_F4, CUT_INT3_KNOCKS, CUT_SUMMIT, CUT_SAYIT, CUT_FEAST_SAVE, CUT_E1, CUT_BALLOON`); flag names for bits 228-255 and the "met" spares (SF §17.1); `DoomoStry` (SF §17.2, exactly 96 B, with a static assert); `story_grumble_*()` and `story_possessor_override(id)` prototypes |
| `core/include/doomo/scenes.h` | S0 → H4 | `SCENE_HOME`, `SCENE_HOME_MART` |
| `core/include/doomo/scene.h` (comment list) | S0 | scratch users: `HOME home*.c` (editor, visit room, defence) and `OWLD ext` (R17) |
| `core/include/doomo/unlock_ids.h` | S0 → I9 | `UL_IDS_HOME_FIRST 564 / _LAST 595` (cut out of action's spare); comments for the story sub-blocks 677..710 and the custom reserve 202..239 (R7) |
| `core/include/doomo/activities.h` | S0 → H4 | `doomo_activities_home` |
| `core/src/stubs.c` | append-only, in marked blocks per spec | weak defaults for every new symbol above (each logs `STUB:`), silent MML placeholders |
| `core/src/save_budget.c` (new) | S0 → I9 | sums each registered chunk's current `size()` + the measured town body upper bound, compares with the slot cap, keeps a 2 KB margin |

**`DoomoStry` (SF §17.2, adopted with one change):** `home_phase` is the single source for the house phase. `HOME` does not duplicate it.

**`HOME` v2 (R9, R10):** HF §9.2 layout with 9 room records, `owned[192]`, `seen_new[48]`, the visitor log (8 B) and `decorate_for` (1 B). The worst case is **3,408 B**, which `size()` reports exactly.

---

## 5. The authoring format for story content (`.dsc`)

Every story writer (S2-S5) writes plain text in `core/data/story/<package-dir>/*.dsc`. `core/tools/textpack.py` (S2) compiles it into `core/gen/text_<pack>.c` (GBA: const ROM arrays), `TEXT.BIN` (PS1: the same bytes; a build product in `ps1/build/`, never committed, C3) and the generated tables `core/gen/side_table.c` and `core/gen/possess_table.c`. `make -C core check-gen` regenerates them byte-identical. The notation is SF §7's, so beat sheets can be copied almost verbatim.

```
@pack ep16 kind=episode chapter=1 minutes=30          # minutes feed story_length_budget
@mission S135 giver=69 side=hill ch=2-3 verbs=find,timing,fetch chain=J step=2 requires=S134
        reward="50 H$" furn=vapor_detector later=10m tag=SCARE

script s135_offer
  say fizz SURPRISE "It beeps at everything. {..}It / also RECORDED the Wall." [HEART]
  choice "Three nights?" { "Sure." -> yes ; "Later." -> end }
yes:
  quest S135 ACTIVE
  push timing pattern=3-1,3-1-2,3-1-2-1 skin=thump -> ok, fail
ok:
  beat {                                    # compiles to BeatEv rows, anchors = labels
    L0:        npc mascotty STILL ; cam pan_to mascotty
    +13        say fizz NEUTRAL "Either she's the only clean / thing in town, or she's not / here." {d}
    L_PUNCH:   hold 13 ; mute 14 ; sfx TIK
    +21        nobutton
  }
  reward 50 furn=vapor_detector
  later 10m { prop house vapor_detector beep_pet }
end
```

**The compiler maps notation to ops:** `[R8]` becomes `+8`. `[HOLD n]` becomes `hold n`. Also mapped: `[MUTE n]`, `[DUCK g n]`, `[TAPESTOP]`, `[SCRATCH]` (a macro: tape-stop 5 + `SHK_BRAKE` + brake + mute 14), `[FREEZE x]`, `[THAW]`, `[PANIC x]`, `[STILL x]`, `[TWITCH x]`, `[INSERT n: what]`, `[SMASH]`, `[COVER n]`, `[FLASH w10,5]`, `[IRIS♥ out h]`, `[POP]`, `[SLAM]`, `[DROP]`, `[JELLY]`, `[BOING]`, `[SQUASH a/b]`, `[SHK_x]`, `[!RIPPLE]`, `[3x@15] {...}` (a macro: three repeats 15 frames apart, with magnitude escalating automatically), `[NO BUTTON]`, `[NO GAG]`, `[DAY n]` and `[NIGHT]`. Line codes `{.} {..} {...} {d} {!} {big}…{/} {w} {r} {punch}`, `/` (line break) and `//` (page break) pass through to the typewriter (SF §15.6).

**The compiler rejects:**
* a page over 3 lines × 28 characters at 240×160, or over 4 lines at 320×240;
* more than one `{d}` in a conversation;
* a jump to an undefined label;
* an undeclared flag;
* a pack over its cap (episode 12 KB, common 6 KB, side 8 KB, Feast 10 KB);
* a mission without `later=`;
* the string "Ten years I" in any file other than the four allowed ones (SF §7.11).

**Ownership rule:** one `.dsc` directory per package (§6). The generated files belong to S2's tool, but their *content* belongs to whoever wrote the input. A failing lint is fixed in the input, never in `core/gen/`.

---

## 6. File ownership

### 6.1 Package to directory

| Package | Owns (new unless marked "edit") |
|---|---|
| **S0** | the §4 headers; `core/src/plat_weak_fx.c`, `core/src/save_budget.c`; the S0 block of `core/src/stubs.c`; edits to `core/include/doomo/{save_ext,scenes,scene,unlock_ids,activities,audio,story,platform}.h`, `core/src/save_ext.c` (2 registry pointers, `max_size`), `core/src/songs.c` (9 rows, R16), `core/tests/test_main.c` (`MAX_TESTS 1024`); `core/tests/test_contracts.c` |
| **S1** | `core/src/tween.c`, `tween_tables.c`, `beat.c`, `camfx.c`, `audio_fx.c`, `sfx_comedy.c`, `ow_spawner.c` (the running-gag spawner + wrongness mechanism), `core/tools/trailer_curves.c` (host tool; writes the committed golden `core/tests/data/trailer_curves.h`); edits to `core/src/ui_dialog.c`, `ui_internal.h`, `cutscene.c` (stages rebuilt on `beat.h`), `overworld.c`, `overworld_npc.c`, `overworld_int.h` (camera field, NPC script slots, `ow_scratch_ext`), `audio.c` (one hook hunk), `audio_int.h`; `gba/src/obj.c`, `gba/src/postfx.c`; edits to `gba/src/video.c` (flip hook, `DCNT_OBJ`), `gba/src/overworld.c` (pan, brake, zoom under a flash); `ps1/src/postfx.c`, `ps1/src/xform.c`; edits to `ps1/src/gfx.c` (second E5, blend TILE hook), `ps1/src/overworld.c` (GTE `H` zoom); tests `test_tween.c`, `test_beat.c`, `test_camfx.c`, `test_dialog_codes.c`; runner `timing_showcase.txt`; `docs/TIMING.md` |
| **S2** | `core/src/scriptvm.c`, `txt.c`, `stry_save.c`, `story_verbs.c` (escort, photo frame check, find markers, deliver slow-down, text entry), `story_timing.c` (the comic-timing press minigame `PEND_TIMING`, skinnable), `unlocks_story.c` (edit), `story.c`, `story_script.c` (gutted to VM glue), `story_talk.c`, `story_places.c` ("Go inside"), `story_questlog.c` (Trophies tab, R8), `story_side.c` (walker over the generated table), `story_int.h`, `dream.c` + `dream_gfx.c` (scripted-dream mode, D1); `mml_mingle.c`, `mml_heehee_jingle.c`; `core/tools/textpack.py`; `core/data/story/common/`, `core/data/story/ch1/`, `core/data/story/ch2/`, `core/data/story/legacy_ch34.dsc` (a temporary port, handed to S3); `core/gen/text_*.c`, `side_table.c`, `possess_table.c` (generated); edit to `core/native/runner_main.c`; `ps1/src/txtload.c`; the `TEXT.BIN` line in the PS1 disc manifest; tests `test_story.c` (maintainer), `test_story_vm.c`, `test_textpack.c`, `test_story_lint.c`; runner `story_p0.txt`, `story_ch1.txt` (edit), `story_int1.txt`, `story_ch2.txt`; GBA `gba/harness/scripts/story_ch1.txt`; `docs/STORY_ENGINE.md` |
| **S3** | `core/data/story/ch3/`, `ch4/`, `epi/`; `core/src/story_summit.c` (the Say It walk, the summit), `story_sprites.c` (procedural non-roster bodies: Sprinklepop's soul, the plush Vapula and jar, Wheezle, Fluffnuts, the Widow, the Gutterking, Signhere, Kevin's bust, GURGLE's portrait slot), `credits.c` (edit), `exorcism.c`, `exorcism_scene.c`, `exorcism_gfx.c`, `exorcism_int.h` (authored possessors + skins), `levels.c`, `levels_play.c`, `levels_draw.c`, `levels_int.h`, `arena_modes.c`, `combat.c`, `combat_hud.c`, `combat_int.h` (photo verb, Vapor Sample drop, boss captions, Grumbleguts mechanics, Breach story entries and the 8 variants of §7 S3), `pet.c`, `pet_scene.c`, `pet_gfx.c` (forms, the pet-care boss, nap and glow flags); tests `test_story_ch34.c`, `test_exorcism_authored.c`; runner `story_ch3.txt`, `story_invasion.txt` (edit), `story_int3.txt`, `story_ch4.txt`, `story_epilogue.txt`; GBA `gba/harness/scripts/story_ch4.txt`; `core/data/home/layouts_s3.txt` (Wisp's room) |
| **S4** | `core/data/story/side_a/` (chains A, B, C, D, S, E, F, G, H, I, J; stand-alones S143-S150), `core/data/story/quests49/` (the 49 existing quests re-timed; #9/#16/#26 retitled), `core/src/story_mg_a.c` (lava hopscotch, the S143 roller counter, the Whack-a-Nothing switched-off mode), `mml_wall_lullaby.c`; `core/data/home/layouts_a.txt`; tests `test_side_a.c`; runner `side_sample_a.txt` |
| **S5** | `core/data/story/side_b/` (chains K, M, N, O, P, Q, R, L, U, T, V; stand-alones S176, S178-S182, S187, S191, S192, S195, S211-S217); `core/src/story_feast.c` (edit; R15), `grumbleguts_gfx.c/.h` (maintainer; additive API only), `story_grumble.c` (R22 getters, dating ticker lines), `story_mg_b.c` (bubble pop, firefly herding, the nap-off breathing rhythm, Clinker's poem rating, the Feast clue rounds and fullness meter); `mml_two_choirs.c`, `mml_boat_floats.c`, `mml_thirteen_bells.c`, `mml_hell_lullaby.c`, `mml_double_act.c`, `mml_number_ten.c`; hook lines in `lib_bestiary.c` / `lib_almanac.c` for Grumbleguts' fate (the library module is finished; its owner hands these lines over); `core/data/home/layouts_b.txt`; tests `test_side_b.c`, `test_side_table.c`, `test_feast.c`; runner `side_sample_b.txt`, `side_chains.txt`, `story_feast_a.txt`, `story_feast_b.txt` |
| **H1** | `core/src/home.c` (state, `HOME` chunk, grants, haunt state), `home_edit.c` (placement rules, stacking, undo ring, multi-select, themes, blueprints, photo recipes), `home_score.c`, `home_defend.c` (wave logic, door HP, turret, guests), `house_api.c` (resident), `unlocks_home.c`; `core/data/furn/catalog.txt`, `core/data/home/themes.txt`, `blueprints.txt`; tests `test_home.c`, `test_home_defend.c`; `docs/HOUSING.md` §1-5 |
| **H2** | `gba/src/room.c`, `gba/src/room_int.h`; edits to `gba/harness/harness.c`, `gba/src/hcmd.c` (R20); the `GBA_BIG_ROOM` id line in `gba/src/gba_plat.h` (after G0 hands it over); `gba/harness/scripts/home.txt`; `docs/GBA.md` housing section (text handed to I9) |
| **H3** | `ps1/src/room.c`, `ps1/src/prims.c` (factored out of `model.c`: the mechanical move lands first, see §6.3); edits to `ps1/src/hcmd.c`, `ps1/harness/run.py`, `ps1/tools/ovl_plan.py` (the `home` group; R4 exclusions); `ps1/harness/scripts/home.txt` |
| **H4** | `core/src/home_draw.c` (`SCENE_HOME`: modes, input, HUD, menus, panels, animation per HF §5), `home_visit.c` (generated rooms, wants, reactions), `home_mart.c` (HomeMart, Black Market, the Workshop furniture bench, `doomo_activities_home`), `room_ref.c` (native reference renderer); `core/tools/furngen.c`; `core/data/furn/art/*.txt`; `core/gen/furn_data.c` (generated); hook lines in `fp_apartments.c`, `fp_resident.c` (care screen "Their home"), `workshop.c` (the bench entry), `lib_gallery.c` (the photo wing); tests `test_home_visit.c`, `test_furngen.c`, `test_home_render.c` (parity golden); runner `home_tutorial.txt`, `home_editor.txt`, `home_rooms.txt`, `home_visit.txt`, `home_haunt.txt`, `home_defend.txt`, `home_mart.txt`, `home_gallery.txt`; `docs/HOUSING.md` §6-9 |
| **I9** | `docs/STORY.md`, `docs/GAME_DESIGN.md`, `docs/PS1.md`, `docs/GBA.md` (merging H2's and S2's text), `README.md`, `ARCHITECTURE.md`, `trailer/README.md` (one line: cues hang off beat labels, never `FR(n)`), `core/tests/README`; `.github/workflows/ci.yml` (only if a new job is needed); `core/tests/test_integration_story_home.c` (`save_joint_worst_case`, the registry and unlock uniqueness extensions, `story_fingerprint` goldens); `ps1/harness/scripts/story.txt`; `core/tests/scripts/integration_story_home.txt`; removal of the S0 stubs that have strong definitions |

### 6.2 Generated files

| File | Generator (its owner) | Inputs (their owners) |
|---|---|---|
| `core/gen/text_*.c` (GBA + native only, C3), `TEXT.BIN` (PS1 build product) | `textpack.py` (S2) | `.dsc` (S2, S3, S4, S5) |
| `core/gen/side_table.c` | `textpack.py` (S2) | `@mission` headers (S4, S5) |
| `core/gen/possess_table.c` | `textpack.py` (S2) | `@possessor` blocks (S2 for Ch2, S3 for Ch3-4, S5 for the Feast) |
| `core/gen/furn_data.c` | `furngen.c` (H4) | `catalog.txt` (H1), `art/*.txt` (H4), `layouts_*.txt` (S3, S4, S5), `themes.txt` and `blueprints.txt` (H1) |
| `core/gen/sfx_data.c` | the existing SFX generator (its current owner) | S1's new recipes in `sfx_comedy.c` are code (PSG recipes) or rows S1 appends to that generator's input. S1 coordinates with the current owner. |
| `core/tests/data/trailer_curves.h` | `trailer_curves.c` (S1, run on the host, output committed) | `trailer/tk.c` (read only) |

### 6.3 Shared files and hand-offs

**The rule:** at any moment a file has exactly one owner. A "hunk" is a single marked block (`/* --- <pkg>: <purpose> --- */ … /* --- end <pkg> --- */`). It is the only edit another spec may make, and only in the order given here.

| File | Other party | Sequence |
|---|---|---|
| `gba/gba.ld`, `gba/src/bigbuf.c`, `gba/src/gba_plat.h`, `gba/tools/memreport.py`, `core/src/scene_scratch.c` | the GBA memory pass (task #77, active now) | **G0:** these stay untouched until #77 merges. After that, H2 adds one line (`GBA_BIG_ROOM`) to `gba_plat.h`, and I9 adds the memreport gates. |
| `core/src/audio.c` | custom B wave 0 (`audio_sfx_pcm()`, SFX to SPU) | **S1 first:** one hunk in the mixer update (global gain and pitch multipliers). Then the custom spec owns the file. If custom wave 0 merges first, S1 rebases its hunk onto it. |
| `gba/src/video.c` | custom wave 1 (palette filter) | **S1 first:** one hunk in `video_flip()` (BG2X/Y, BG2PA-PD, BLDY, OAM DMA) and one `DCNT_OBJ` bit. Then the custom spec owns the file. |
| `ps1/src/gfx.c` | custom wave 1 (palette filter) | **S1 first:** one hunk (the second E5 before the HUD, a blend TILE call). Then the custom spec owns the file. |
| `ps1/src/model.c` | custom wave 2 (fat body, head impostor) | **H3 first:** move `box()/blob()/cylinder()/pyramid()` into `prims.c`, with behaviour unchanged and pixel-identical goldens. Then the custom spec owns `model.c`. |
| `core/src/sim_soul.c` | custom wave 0 | the custom spec owns it. S3 provides `story_possessor_override(id)` (strong in `exorcism.c`, weak in stubs) and **the custom spec inserts the one call** in its pick function. If custom wave 0 has not started when S3 needs it, S3 inserts the hunk and hands the file over. |
| `core/src/stubs.c` | everyone | append-only blocks, one per spec. I9 deletes blocks whose symbols are all strong. |
| `core/include/doomo/save_ext.h`, `save_ext.c` | custom (A adds chunks; B adds `max_size`) | **S0 first:** adds the two pointers and `max_size`. The custom spec appends after it. |
| `core/include/doomo/unlock_ids.h` | custom (foundation 202..239) | **S0 first:** records home 564..595, the story sub-blocks and the custom reserve. The custom spec fills its reserve. |
| `core/include/doomo/platform.h` | custom (palette filter hooks, if any) | **S0 first** (additions only). The custom spec appends. |
| `core/native/runner_main.c` | custom (its own commands, if any) | S2 adds one hunk; the custom spec adds its own hunk. |
| `ps1/tools/ovl_plan.py` | custom (group sizes, not regexes) | H3 owns the `GROUPS` list. The custom spec asks H3 for any regex change. |
| `core/src/fp_apartments.c`, `fp_resident.c` | custom (fp screens; design B only touches `fp_common.c`) | H4 hunks only (push `SCENE_HOME`). |
| `core/src/town.h` | custom | **no edits from this SPEC.** The `town.h:73` comment fix (SF §13b) goes to the custom spec's owner as a request. |
| `core/include/doomo/resident.h`, `sim_*.c`, `portrait_*.c`, `creator*.c`, `options.c`, `input.c`, `save.c` | custom | **no edits from this SPEC** (R22). Creator missions (S149, S162, S213) use the existing `scene_push(&SCENE_CREATOR, arg)` API only. |
| `core/src/songs.c` | the story owner today | S0 adds the 9 rows (R16). After that nobody edits it. |

---

## 7. Work packages

Every package must meet the **common acceptance criteria** (CA) below, plus its own.

**Common acceptance (CA), for every package's merge:**
1. `make -C core test` passes: unit tests plus every runner script at 240×160 **and** 320×240, with `# assert-no-stubs` on each new script.
2. `make -C core test SAN=1` passes (ASan + UBSan clean).
3. `make -C core xcheck` passes: gcc, arm-none-eabi and mipsel, with `-Werror`.
4. `make -C core check-gen` regenerates every generated file byte-identical.
5. `make -C gba` builds the ROM and `make -C gba test` passes. Memreport gates (§9) hold.
6. The PS1 game disc builds (`cmake --preset ps1`). The `ovl_plan.py` report holds every group ≤ the package's group cap, and **no group grows past today's biggest group** (C4). The PS1 resident ledger (C5) holds: `elf2exe.py`'s figure stays ≥ the G0 baseline − the package's ledger allowance, and never below 72 KB below the stack top.
7. GitHub CI (`core`, `gba`, `ps1` jobs) is green on the PR.
8. Every new screenshot was looked at by eye at both resolutions (the project rule).
9. No new floats, `malloc` or stdio in `core/`. No new IWRAM code. Integer math only. Everything advances on the 30 Hz tick.
10. Owned files only, plus the hunks named in §6.3.

---

### S0: Contracts and registries

* **Scope:** §4 complete. Every new symbol links through a weak stub that logs `STUB:`. R7, R13, R14, R16 and R21 are applied.
* **Depends on:** nothing. This package unblocks all the others.
* **Deliverables:**
  * the headers;
  * `plat_weak_fx.c`: software camfx fallbacks and a `plat_image_xform` that calls `plat_image` at scale 1 when it has no hardware path;
  * `save_budget.c`;
  * the registry edits;
  * the stubs;
  * `test_contracts.c`, which checks:
    * `sizeof(DoomoStry) == 96`, `sizeof(BeatEv) == 8`, `sizeof(StoryOp) == 4`, `sizeof(DlgLine) == 6`, `sizeof(DoomoRoomObj) == 12`;
    * the unlock ranges are disjoint;
    * the new registry pointers are read through a volatile pointer;
    * `save_budget_left()` is monotonic as chunks grow;
    * `MAX_TESTS ≥ 1024`.
* **Acceptance:** CA. No behaviour changes: every existing runner script and golden is unchanged.
* **Budget:** about 3 KB of headers. 0 B RAM at runtime (unused stubs are removed by `--gc-sections`).

---

### S1: The comic timing engine and its integration (cutscene, dialogue, overworld, platform post-FX)

* **Scope:**
  * `tween` (CT §7.1 + SF §15.4), with the springs and their presets;
  * `beat` (CT §7.2 + SF §15.3 + R5), with `beat_age` and `beat_clock`;
  * `camfx` (CT §7.3, Appendix C), with the 14 profiles and the palette lerps (sepia, crayon, hellify, lilac);
  * the audio hooks (mute, duck, tape-stop, resume-on-beat, dual music, SFX pitch), the 13-sound comedy kit, GURGLE's voice preset and the pluck;
  * the dialogue upgrades (SF §15.6): inline beat codes and punctuation holds, the per-glyph hop, typed / popped / appearing lines, the portrait talk bob and shout stretch, the deadpan clock clamp, 11 expressions from the line record, `TWITCH` for possessed speakers, the name pill only on `PILL` lines, the GURGLE secondary portrait slot;
  * cutscene stages (SF §15.7): the backdrop, 8 actor slots, the camera, the palette, `BOP_SHOT` switches, `START` skipping the whole cutscene, `A` never shortening a `HOLD` after `L_PUNCH`. The 11 existing cuts are rebuilt on `beat.h` with their art kept. `CUT_FEAST_POP` keeps calling `feast_tick/feast_draw` until S5 ports it;
  * overworld scripting (SF §15.8): the NPC states (`WALK_TO, HOP, TURN, FACE, EMOTE, FLEE, FREEZE, THAW, PANIC, STILL, TWITCH, HEAD_TURN`); the camera field (pan, zoom, creep, brake); the double-take; the border record scratch; the spawner table `{gag, weight, cooldown_min, chapter_mask, night_only}`, rate-limited to one per 20-40 s with 5-15 s of quiet after a big beat and never during a `NOBUTTON` beat; the wrongness mechanism W1-W16 with `noticed` bits (the rows are content: S2 writes them as data in `ch1/`, `ch2/`); `ow_scratch_ext` (R17);
  * platform post-FX: GBA BG2 offset, scale and shear at flip, BLDY flash and fade, palette DMA lerp at vblank, software iris spans, affine-OBJ `plat_image_xform` for up to 4 hero actors per frame (the rest draw unsquashed), MOSAIC for glitch; PS1 E5 offset with a second E5 before the HUD, ABR TILEs for flash, fade and colour, GTE `H` zoom, row-TILE iris, `POLY_FT4` xform;
  * reduced-motion and safety rules (CT §9): options scale amplitude only, never timing; full-screen flashes stay under 3 Hz.
* **Depends on:** S0. Lands its shared-file hunks before custom waves 0-1 (§6.3).
* **Order inside S1:**
  1. M1: `tween`/`beat`/`camfx` + tests + the GBA/PS1 post hooks (this unblocks everyone);
  2. M2: audio hooks + SFX kit;
  3. M3: dialogue codes + pill + portraits;
  4. M4: cutscene stages + rebuilding the 11 cuts;
  5. M5: NPC/camera scripting + spawner + wrongness.
* **Acceptance (CA, plus):**
  * `tween_matches_trailer`: `tw_bounce` within 0.2% of `tk_bounce` at 33 points; the `TW_*` tables equal CT Appendix A; the spring step responses match `trailer_curves.h` within ±1 Q4.
  * `test_beat`: events fire at `label_frame + at`; A moves anchors only; `beat_skip` applies flags only; `beat_clock` freeze / 2x / normal; the cost is O(events fired).
  * `test_camfx`: every profile's samples equal CT Appendix C; `OPT_SCREEN_SHAKE` 0/50/100% scales amplitude only, so the same frames are marked; at `OPT_REDUCE_FLASH=1` no level exceeds 6/16 and no red flash remains.
  * `test_dialog_codes`: holds per punctuation mark; `{d}` appears whole; `{punch}` cannot be skipped; the pill is drawn only on `PILL` lines; a 200-character decoded line fits the queue entry.
  * Runner `timing_showcase.txt` replays three trailer gags in-engine (s04's pet turn, the s12 census counter, the s14 handheld DROP) with shots on the trailer's key frames, compared by eye against frames from a `trailer/` build.
  * The existing `story_cutscenes.txt` and `story_ch1.txt` shots are re-approved by eye: timing changes, content does not.
  * GBA: `fps cutscene_wall` ≥ 28. `memreport`: S1's `.bss` ≤ 400 B, IWRAM +0 B.
  * PS1: `fps` ≥ 28 on the rebuilt `CUT_WALL_FALLS`; the `cutscene` group ≤ 100 KB.
* **Budget:**
  * Code: ≤ 14 KB (tween 2, beat 3, camfx 2.5, audio 1.5, dialogue 1.5, NPC/camera 3, spawner 0.5) **resident** on PS1, plus the stage renderer ≤ 8 KB **in the `cutscene` group**.
  * Data: tables ≤ 0.5 KB, profiles 0.25 KB, palettes 2 KB.
  * SFX: GBA ≤ 8 KB (PSG recipes where possible). PS1 ≤ 30 KB ADPCM; it goes to SPU RAM if custom's `SFX.BIN` lands, otherwise it counts as resident and I9 tracks it.
  * EWRAM: beat 96, camfx 48, springs 32, cooldowns 32, OAM shadow 256 = **464 B**. NPC slots live in `OwScratch` (0 net).

---

### S2: The story engine, Chapters 1-2, P0, Interlude I, F1, D1

* **Scope (engine):**
  * `scriptvm.c` (SF §15.2 + `LATER`) and its sub-scene push/resume glue (`PUSH` saves the pending kind and resume label; the result is in var 0);
  * `txt.c`: canonical-Huffman decode with a static 512-entry dictionary (SF §16.3); the GBA uses ROM pointers, the PS1 the resident 36 KB window (R2) and `ps1/src/txtload.c` (`cd_read_range` from `TEXT.BIN`);
  * `textpack.py` (§5), including all of its lints;
  * `stry_save.c` (the `STRY` chunk, 96 B);
  * `story_rng`, `story_fingerprint`;
  * `story_verbs.c`, `story_timing.c` (the timing minigame: patterns, metronome, laugh-track skins, the drift mode for S155);
  * the side-table walker over `side_table.c`: chain gating (step k requires k−1; at most one part per chapter; cross-border parts 3+ from Ep 4.1), Hillvile givers on the invaded map in Ch3;
  * the quest-log Trophies tab (R8), "Go inside" at `PLACE_HOME`;
  * unlocks at 677..710 (R7);
  * dream.c's scripted-dream mode (floating letters, a forced Peek);
  * the runner commands `seed`, `expect-text`, `expect-flag`;
  * porting the 28 existing beats to scripts, so the existing `test_story.c` passes unchanged (old `BEAT_*` ids alias onto episodes). The Ch3-4 port goes in `legacy_ch34.dsc`, which is handed to S3.
* **Scope (content):**
  * P0 (SF §9), Ch1 Ep 1.1-1.6 (§10), Interlude I, Ch2 Ep 2.1-2.7 (§11), F1, D1, the Bottle;
  * the chapter-common packs for Ch1-2: idle lines, region lines, W1-W16 rows with their captions, the spawner rows (the starer, the crowned cloud, "beep." during a sincere line, the Days Since flip, Grumbleguts' Ch2 insert);
  * the Ch2 authored possessors (Steamy/TALLYWORN; Sprinklepop/WHEEZLE as a scripted fail) as `@possessor` blocks;
  * house beats through `HOME` ops: the Ep 1.2 key and tutorial (COSY), the Ep 1.5 housewarming (nine gifts, 8 frames apart), the Ep 2.3 haunting nights 1-3 (LIT, the wallpaper eye), the Ep 2.7 Cellar;
  * `SONG_MINGLE` (the music box, the hummed version, the full version, forwards and reversed for the two-faith rite) and the "Hee. Hee." jingle;
  * NG+ lines for Ch1-2.
* **Depends on:**
  * S0 and S1 M1 (beat/tween). S1 M3 is needed for the dialogue codes; until then, codes render as plain text.
  * H1 M1 (`house_api.c`) for the house beats. Until then the stubs auto-pass goals, and those beats are marked in `story_ch1.txt` with `expect-scene HOME` only after H1 M1.
* **Acceptance (CA, plus):**
  * Unit tests (SF §18.1): `story_full_playthrough_reaches_the_credits` (kept; through Ch2 with the new scripts, the rest on the legacy port), `story_debug_jump_sets_up_every_beat` (all 36 new ids have a setup), `story_vm_scripts_valid`, `story_text_fits_the_box`, `textpack_roundtrip`, `beat_script_timing` (the lint), `silence_budget` (Ch1-2: 20-25% of frames without music), `story_style_rations`, `story_save_round_trip_mid_story` (STRY at every episode boundary; `size()` == bytes written; exactly 96 B), and the new **`story_length_budget`** (declared `minutes` per chapter within ±10% of SF §8: Ch1 150, Ch2 180).
  * Runner `story_p0.txt`, `story_ch1.txt`, `story_int1.txt`, `story_ch2.txt` with the shots listed in SF §18.2.
  * GBA `gba/harness/scripts/story_ch1.txt` replays Ep 1.1 and Ep 1.6 with `fps` ≥ 28 in cutscenes. `expect-flag` matches native.
  * PS1: `expect-log "txt: pack ep11"` at the episode boundary. Resident code from `scriptvm.c` + `txt.c` ≤ 9 KB; the window is exactly 36 KB. The story group ≤ 75 KB.
* **Budget:**
  * ROM: Ch1-2 + P0 + specials text ≤ 26 KB packed, scripts ≤ 18 KB, beats ≤ 20 KB, the shared dictionary and Huffman table ≤ 2 KB.
  * EWRAM: VM 64 + txt 16 + the `STRY` live copy 96 = **176 B**.

---

### S3: Chapters 3-4, Interlude III, F2-F4, D2, the epilogue and credits; exorcism, combat and pet hooks

* **Scope (code):**
  * the authored possessor mechanism (SF §15.11): the override hook, every skin (Hattie/HAZE, whose prayer phase is the timing minigame; Grampy/SOOTKIN, whose name phase is the tomato memory game; Smog/FLUFFY, with the VAPULA near miss, the GIGGLESWORTH strike and the two-faith rite with the song reversed on the second channel; Sprinklepop's reverse rite "EXORCISM UNFAILED", where the player types the name; Mr. Huggins 2nd/4th/9th practice skins; S211's single-prompt wisp). Losses have consequences (SF §13b).
  * Combat: the photo verb; the Vapor Sample drop; boss captions (`lvd_boss_intro` for Vapula's seven floors and Grumbleguts); the "it's fine" pellets and Hee-Hee puffs; rimshot and spoon pickups; Grumbleguts' mechanics (the belly weak point, feed-to-stun with rating captions, the belly-flop with GURGLE's warning, burp portals in threes, the trampoline, the 3 f rear-wiggle telegraph); `USTORY_PACIFIST` granted for real; story entries for Breach 1, 2, 7 linked into level select (`integration_todo`); **the 8 variants requested by S5:** Bakery (Breach 3), Soft (4), Dry (5), Choir (6), the Ninth Cloud remix map, Arena "Exes", Arena "Hold the Door" (which needs H1's export), and Breach 3-7 post-story entries.
  * The pet: forms (nubs after Interlude I, horns after Ep 3.7, halo tint), the 1-frame and 4-frame inserts, the pet-care boss (SCENE_PET under fire), "beep." as an answer, the demons-flinch aura, the nap and glow flags (consumed by S5).
  * `story_summit.c`: the Say It walk, a pool of about 60 lines + 36 cast lines picked per friend by hash, the summit.
  * `story_sprites.c`.
  * `credits.c`: the parade with the pet horned, Wisp in a body, Vex with the jar, Grumbleguts at the roadside unless `SF_GRUMBLE_DEAD`.
  * The Ch3-4 boss text in `levels.c` (SF §18.5).
* **Scope (content):**
  * Ch3 Ep 3.1-3.7 (SF §12), F2, F3, Interlude III (EMPTY phase; remove-only; seven knocks; `[NO MUSIC]` exempt from the silence budget);
  * Ch4 Ep 4.1-4.7 (§13), D2, F4;
  * the epilogue E1-E2 and the title variants;
  * the Say It pool, nest region lines, the Ch3-4 chapter-common packs, NG+ lines for Ch3-4;
  * the siege's chimney `BeatScript` (run by the house through `beat.h`);
  * Wisp's room layout (`layouts_s3.txt`) for `DECORATE_FOR`;
  * the SF §13b continuity fixes (pronouns, `L_help_done` branching, Steamy "she", the Mascotty-is-the-starer rule, the Fluffy grave text);
  * takes over `legacy_ch34.dsc` and deletes it when Ch3-4 are complete.
* **Depends on:** S0, S1 M1-M4, S2's engine (VM, txt, textpack). H1 M2 (`home_defend`) for Ep 3.2 / 3.7, and H1 M1 for Interlude III and Ep 4.4. **S3 M1 (the authored possessor hook + `possess_table.c` reading) is due early**, because S2's Ch2 rites consume it.
* **Acceptance (CA, plus):**
  * Unit: `story_possessors_authored`, `story_pet_forms` (11 expressions × 3 forms), `story_choice_consequences` (the write/read table of SF §8.2 across Ch1-4), `story_full_playthrough_reaches_the_credits` through all new scripts with no legacy port left, `silence_budget` and `story_length_budget` for Ch3 210 and Ch4 210 minutes, the epilogue ≥ 25 minutes.
  * Runner `story_ch3.txt`, `story_invasion.txt` (extended), `story_int3.txt`, `story_ch4.txt`, `story_epilogue.txt` with the SF §18.2 shots.
  * GBA `story_ch4.txt`: `fps summit` ≥ 28, `fps cutscene_wall` ≥ 28.
  * PS1: `fps` ≥ 28 on the summit. Group caps: `exorcism` ≤ 100 KB, `combat` ≤ 108 KB (measure first; if over, move the boss text into the pack), `pet` ≤ 100 KB.
* **Budget:**
  * ROM: text ≤ 36 KB packed, scripts ≤ 24 KB, beats ≤ 28 KB, `story_sprites.c` art ≤ 40 KB.
  * Code ≤ 26 KB across the groups.
  * EWRAM: the possessor working row 24 B.

---

### S4: Side missions, batch A (54 missions + the 49 existing quests re-timed)

* **Scope:**
  * Chains A (S100-S102), B (S103 S104 S105 S202 S203 S106), C (S196 S107 S108 S197 S198 S109), D (S199 S110-S113), S (S114-S116), E (S117 S118 S201 S119 S120), F (S121-S123), G (S124-S126), H (S127 S200 S128 S129), I (S130-S133), J (S134 S135 S204 S136);
  * stand-alones S143-S150;
  * all as `.dsc` with `@mission` headers, the comic beat, the heart or scare beat, the reward and the "Later";
  * the 49 existing quests (#0-#48) moved to `.dsc` with beat codes (#9 "Pier Panorama", #16 "Streamer Scream", #26 "Spider Ride");
  * `story_mg_a.c`;
  * `SONG_WALL_LULLABY` (S148);
  * the decorate-for layouts `layouts_a.txt` (Pip's shoebox bed S100, Steamy's bath house S128, the Towers 7B flat S114-S116, Mascotty's room S136, the shared flower bed S126);
  * the furniture reward keys used must exist in `catalog.txt` (ask H1 for any missing ones before writing).
* **Depends on:** S2's engine (VM, textpack, verbs, timing minigame, side-table walker), S1 M3, H1 M1 for the housing verbs. The chapter content of S2/S3 must exist for the chapter windows, but the missions can be authored and tested with `scene STORY <beat>` jumps first.
* **Acceptance (CA, plus):**
  * `test_side_a.c`: S100 (housing), S101 (escort/photo/timing), S118 (practice rite), S133 (reverse rite), S135 (three nights), S200 (no music). Each runs through the real talk path with autowin, gets its reward, and fires its "Later".
  * Every batch-A mission passes the lint (Appendix C).
  * Runner `side_sample_a.txt`: one shot per sampled mission on its punch frame, at both resolutions.
  * The existing `story_side_quests_table` (old table) still passes.
* **Budget:** ROM text ≤ 62 KB packed (≤ 8 KB per side pack), scripts ≤ 20 KB, beats ≤ 6 KB, `story_mg_a.c` ≤ 6 KB of code (story group). 0 B EWRAM.

---

### S5: Side missions, batch B (55 missions), quest-chain verification and the Feast

* **Scope:**
  * Chains K (S137-S140), M (S151-S153), N (S154-S156), O (S157-S159), P (S142 S161 S162), Q (S163-S165), R (S166 S167 S205 S168 S206), L (S169-S171), U (S172-S174), T (S183 S184 S207 S185 S186), V (S208-S210);
  * stand-alones S176, S178-S182, S187, S191, S192, S195, S211-S217;
  * `story_mg_b.c`;
  * 6 songs (Two Choirs, Boat Floats, Thirteen Bells, Hellvile Lullaby, Double Act, Number Ten);
  * `story_grumble.c` (R22): friendship, romance and dating-ticker lines, kids off-limits (SF §3 rules 1-5), `RES_DEAD` and the no-rite lock read by the graveyard;
  * **the Feast S186**: weekend-only (`day % 7 == 6`); guests from the friends list in `ow_scratch_ext`; the culprit drawn by hash, never a kid, the Mayor or Nox; the clue rounds; the fullness meter on belly and rear with strap pings on steps 3 and 6 and a chair giving way on 7; the exorcism or "let's see what happens" choice; `CUT_FEAST_SAVE` (new) and `CUT_FEAST_POP` **trimmed to SF's staging and ported onto `beat.h` stages (R15)**; the permanent consequences (`STRY.feast = 2`, `SF_GRUMBLE_DEAD`, `SF_GRUMBLE_NO_RITE`, the headstone, the memorial dish, the credits cameo removed, the bestiary/almanac hook lines);
  * the decorate-for layouts `layouts_b.txt` (the two-door clinic, the gatehouse, the Florist's stall, Tallyworn's second wall, the Annex for S210);
  * `test_side_table.c`, which owns the table-wide content checks: 118 rows, 109 live; every verb used; ≥ 45 Hillvile and ≥ 35 Hellvile givers; chapter counts 17/19/18/32/23; chains monotone; givers reachable in their window; retired ids are `KIND_NONE`; 118 × 2 ≤ 240 bits; `NOBUTTON` endings never twice in a row in a chain; exactly 4 news-ticker "Later"s (S140, S156, S168, S191).
* **Depends on:** the same as S4, plus S3's variants (Breach 3-6 variants, the Arena "Exes" mode, the Ninth Cloud map) for S171, S174, S178-S181.
* **Acceptance (CA, plus):**
  * `test_feast.c` (`story_grumbleguts_feast` with a fixed seed: the culprit rules; the meter fills in 9 courses; Branch A alive and friendly; Branch B sets `RES_DEAD`, both `SF_GRUMBLE_*` bits, the headstone and the memorial dish, and `graveyard_rite(grumbleguts)` refuses).
  * `test_side_b.c`: S142 (timing), S164 (text entry), S166 (haunting), S183 (cooking + the belly's verdict), S186 (both branches, seeded), S206 (no comic beat), plus S211 (`[NO GAG]`).
  * `test_side_table.c`.
  * Runner `side_sample_b.txt`, `side_chains.txt`, `story_feast_a.txt`, `story_feast_b.txt` (`seed 7`; `opt gore 0` and `opt gore 1`) with the SF §18.2 shots; **the shot list of `story_feast_b.txt` matches SF S186 §5 and contains no frame from the cut panels.**
  * GBA: `fps feast_pop` ≥ 24 with 60 confetti particles. PS1: ≥ 22 in Redux.
* **Budget:** ROM text ≤ 64 KB packed, the Feast pack ≤ 10 KB, scripts ≤ 20 KB, beats ≤ 8 KB, `story_mg_b.c` ≤ 8 KB. `story_feast.c` must not grow (it shrinks with the cut). EWRAM 0 B (the Feast's state is in `ow_scratch_ext`, 256 B).

---

### H1: House core (data, catalog, editor logic, save, story and sim hooks)

* **Scope:**
  * `home.c`: the state (`HOME` v2, 3,408 B worst case); `size/write/read/reset` with `max_size`; the joint budget call (R13); grants (`home_grant` scanning the `{unlock, furn}` table at load, so no resident module points into the group); the Ch2 haunt state (the rule of three over three nights, the 3 AM move, the persistent wallpaper eye); phase transitions read from `STRY.home_phase`.
  * `home_edit.c` (HF §2): the placement rules and the "nope"; stacking and half-tile snapping; the fireplace rule; the immovable door slot; rotation; recolour; the 32-step undo ring; multi-select (up to 8) and the clipboard; 10 themes; 12 ROM + 2 saved blueprints with ghost shopping lists; 8-byte photo recipes (16 slots); remove-only mode (Interlude III); gift tags as catalog properties.
  * `home_score.c` (HF §4): Comfort, Spook, Style/Harmony, Light, Barricade; the feng-shui rules; grades; the rating rule of three.
  * `home_defend.c` (HF §10.3, retimed by R11): the Ep 3.2 red-sky wave of 3 imps; the Ep 3.7 siege of 3 waves (6 imps; 10 imps + 1 brute; 14 imps + 2 brutes + the chimney beat); door HP = Barricade; turret re-winding; the guests' "safe" counter; HELD; `SF_HOUSE_*`; Night Raids; and the arena export used by S3's "Hold the Door" mode.
  * `house_api.c` (resident).
  * `unlocks_home.c` (564..595).
  * `catalog.txt` with 306-330 rows, including every SF furniture reward (R9).
  * `themes.txt`, `blueprints.txt`.
  * The sim effects of the stats (sleep restore, pet holiness drift, visitor happiness, Night Raid chance) as functions the existing sim calls through `house_api.c` getters. No edit to `sim_*.c` (R22): the story or free-play scene that already runs the night applies them.
* **Depends on:** S0, and **G0** (task #77 merged) before `HOME`'s `.bss` lands on the GBA. The fallbacks if G0 slips (HF §6.5): one saved blueprint (−250 B), regenerated `visited` (−62 B), 32-object rooms (−288 B).
* **Order inside H1:**
  1. M1: state + chunk + `house_api` + placement + score (this unblocks S2-S5 and H4);
  2. M2: defence;
  3. M3: themes, blueprints, photos, haunting.
* **Acceptance (CA, plus):** `test_home.c` (HF §11.1: `home_catalog_valid`, `home_place_rules`, `home_undo_ring`, `home_save_roundtrip` (empty house 10 B when not moved in; worst case exactly 3,408 B), `home_save_caps` (story ≤ 40 KB), `home_score`, `home_photo_recipe`, `home_registry`); `test_home_defend.c` (`home_defend_waves`: the sums match; door HP = Barricade; with no input, wave 3 loses exactly the Mayor's "safe"; the chimney beat fires on wave 3). GBA `memreport`: `HOME` ≤ 3,408 B.
* **Budget:**
  * Code ≤ 35 KB in the `home` group on PS1 (`home.c` 14, `home_edit` 10, `home_score` 3, `home_defend` 8). `house_api.c` ≤ 1 KB resident.
  * Catalog data ≤ 9.5 KB (330 × 28 B).
  * EWRAM 3,408 B. Scratch ≤ 2.5 KB of the house's 12 KB.

---

### H2: The GBA isometric renderer (platform side)

* **Scope:**
  * `plat_room()` per HF §6, all of it: the 2:1 projection with 32x16 diamonds; view rotation in the core; multi-tile pieces split into columns; depth keys with an insertion sort rebuilt only on `gen` changes; walls from 16 px strips; the Garden sky table.
  * A static bitmap with dirty rects on both pages: up to 12 per page, else a full recompose; UI bboxes fed back into the next frame's list.
  * The LUT recolour (6 marker indices × light × tint) cached in `gba_big`; light pools; window glass.
  * OBJ for the cursor, the held ghost (affine DROP, nope and squash-flip), emotes, "!" and stickers, through S1's `obj.c` with `obj_set_shadow(buf, 128)`.
  * BG2 shear for 2 frames on a rotate; BG2 zoom 1.14x in photo mode.
  * Chibis in the bitmap, so they occlude correctly.
  * `plat_draw_skipped()` honoured.
  * The `gba_big` claim `GBA_BIG_ROOM` (≤ 11.2 KB).
  * The harness commands (R20).
* **Depends on:** S0 (the platform API), S1 M1 (`obj.c`, post-FX), H4's `furn_data.c` (iso sprites, span-encoded). A 20-piece stand-in set from H4 M1 is enough to start. G0 is needed for the `gba_plat.h` line.
* **Acceptance (CA, plus):**
  * `gba/harness/scripts/home.txt` (added to `make -C gba test`): `fps home_steady` ≥ 28, `fps home_rotate` ≥ 24, `fps home_cellar_full` (8x8, 40 pieces) ≥ 20, `fps home_defend_w3` ≥ 20, `fps home_modal` ≥ 20.
  * `profile 2000000 plat_room`.
  * `arm-none-eabi-nm` shows no new IWRAM symbol.
  * `memreport`: the `gba_big` claim ≤ 17,408 B, OBJ VRAM ≤ 10.5 KB.
  * The GBA harness shot of the fixed layout equals H4's `room_ref.c` golden pixel for pixel, with OBJ hidden (HF §11.5).
* **Budget:** THUMB ROM code ≤ 12 KB, 0 B IWRAM, `.bss` ≤ 64 B (everything else in `gba_big` or scratch). Frame time: steady 11-13 ms, a rotate ≤ 20 ms, worst case ≤ 50 ms for at most 2 frames.

---

### H3: The PS1 2.5D renderer (platform side)

* **Scope:**
  * HF §7: the room box from the grid (64 units per tile, 128 units for walls), the cutaway with a 4-frame drop-and-pop when the stubs swap, the ceiling plane in orbit and photo mode.
  * Furniture from the same `FurnPrim` lists through `prims.c`; billboards for `FF_BILLBOARD` pieces with independent x/y squash; the player as the real 3D chibi; visitors as `figure_render` billboards.
  * The diorama camera: pitch 35°, yaw 45° + 90°k on a `TW_BOING` spring, free orbit on L2/R2, two zooms, a +12% punch through `camfx`.
  * Lighting: day colour, sun or moon, 4 lamp pools on floor vertices from a 64-entry table; contact shadows.
  * CLUT recolours (borrowing 4 silhouette rows); hell tint and jitter.
  * **The partial static-vertex cache in `plat_cache` (R1)**: 8 B per vertex, at most 1,024, filled floor first, then walls, then the largest pieces.
  * The `home` overlay group in `ovl_plan.py` (`home(_\w+)?$|furn_data$|room_ref$`, excluding `house_api`) plus the R4 exclusions.
  * The harness commands.
* **Depends on:** S0, S1 M1, H4's `furn_data.c` prims. **The `prims.c` factoring is H3 M0 and lands before custom wave 2.**
* **Acceptance (CA, plus):**
  * `ps1/harness/scripts/home.txt`: `fps home_steady` ≥ 28 in Redux (target 30 on a console), `fps home_orbit` ≥ 22, `vram home_cluts`, `dshot` of the DROP mid-squash and the orbit overshoot, `expect-log "ovl: home"`, `--assert-no-stubs`.
  * `ovl_plan.py` keeps nothing from `home*` resident except `house_api.c`.
  * `memcard_check.py`: the test house fits in 2 blocks.
  * M0: `model.c` output is pixel-identical before and after the factoring (existing goldens and dshots).
* **Budget:** resident `room.c` + `prims.c` ≤ 8 KB. The `home` group ≤ 100 KB target, 108 KB hard; if over, cut in this order: shared phrases for `desc` strings (−6 KB), then move `home_defend` tables into the pack. VRAM stays in the existing cache pages. Cache ≤ 8 KB in scratch.

---

### H4: The house scene and editor UI, visiting citizens, furniture art

* **Scope:**
  * `home_draw.c` = `SCENE_HOME`: Live / Build / Photo modes and their controls (HF §2.1); the 12 px HUD bars; the START menu modals (catalog, wallpaper, blueprint, theme, photo, score); the Score panel explaining the feng-shui rules; **every animation of HF §5** through `tween`/`beat`/`camfx`: the heart iris, pick-up anticipation, the hover ghost, DROP soft with dust, the neighbour ripple, the nope, the squash-flip, the brake on a rotate, covered cuts with the paint-roller runner, the group DROP stagger, the reverse DROP for undo, the weld-montage blueprint gaps 10/9/8/8/6/4, the room-expansion rumble, the counter + SLAM grade stamp, the shutter; the baseline idles; animated and gag pieces; the pet living in the house; scares and whiplash inserts with one persistent change; the heartfelt sleep with no button; the house vamp ducking to 0.6 under bubbles and tape-stopping when a cursed piece lands.
  * `home_visit.c` (HF §3): deterministic generated rooms for the 1000 citizens; the possessed, ghost, dead (the sealed room), away and relationship variants; 32 gifts; wants; personality reactions (the double-take, the dawning take, the deadpan, ripples, flees, sitting).
  * `home_mart.c`: HomeMart (12 seeded stock, the 30% deal, locked "???" rows, the Wants tab, seasonal rotation, Last Season at 3x), the Black Market (night only, then day after Ch4), the Workshop furniture bench (reached by a push), `doomo_activities_home`.
  * `room_ref.c`.
  * `furngen.c` (integer-only, byte-identical on every host, `check-gen`, `--sheet` contact sheet) + `core/data/furn/art/*.txt` for every catalog row; span-encoded 8bpp facings for the GBA; prims for the PS1; footprint masks.
  * The free-play hooks (the facade → visit; "Their home"; the cottage on the Hillvile facade; Town Hall "Expand"; the gallery photo wing; the almanac's `house_points`).
* **Depends on:** S0, S1 M1-M3, H1 M1. Art M1 = the first 60 pieces (the Ch1 set + HomeMart basics) to unblock H2 and H3; M2 = all pieces.
* **Acceptance (CA, plus):**
  * `test_home_visit.c` (`home_visit_determinism`: all 1000 ids on two seeds, a golden FNV, never a fear trigger; `home_visit_time` < 60k ops per generation).
  * `test_furngen.c` (unmatched keys fail; marker paints are respected; the art ROM total ≤ 0.8 MB; average ≤ 2.1 KB per piece).
  * `test_home_render.c` (the `room_ref` golden).
  * Runner scripts `home_tutorial.txt`, `home_editor.txt`, `home_rooms.txt`, `home_visit.txt`, `home_haunt.txt`, `home_defend.txt`, `home_mart.txt`, `home_gallery.txt` with the HF §11.2 shots at both resolutions.
  * The density rule: 1 reaction per placement, then a cooldown.
* **Budget:**
  * ROM art ≤ 0.8 MB on the GBA. If it goes over: drop back facings except the 20 that need them; bake 2 animation frames instead of 4; then LZ-pack sprites per room into the `gba_big` cache.
  * The `home` group on PS1: `home_draw` 10 KB, `home_visit` 6 KB, `home_mart` 6 KB, plus `furn_data` prims and strings ≤ 28 KB. Together with H1's code this stays under H3's group cap.
  * Scratch ≤ 1.5 KB.

---

### I9: Integration and verification (the last package)

* **Scope:**
  * Merge the packages in the order of §8. Run every gate in §9 on the merged tree.
  * Write `save_joint_worst_case` (R13) and the `story_fingerprint` goldens per runner script, with native, GBA and PS1 asserting the same fingerprint after the same script.
  * Timing parity: the harnesses log the tick each `L_PUNCH` fires; it must equal the native tick. A second pass at `opt reduce_flash 1` must show only amplitudes changed.
  * The whole-game playthrough `integration_story_home.txt`: P0 → credits → E2 → one Feast branch, with no stubs.
  * The PS1 `story.txt` script (Ep 1.1, 2.5, 3.7, 4.6, the Feast) with `expect-log "ovl: story"` and `"txt: pack ep46"`.
  * The docs (SF §18.5, HF docs): the play-time target in `GAME_DESIGN.md` is 12-14 h main and 25-35 h with side content and housing.
  * The README status table; removing stubs that now have strong definitions.
  * Checking the 13 unlock-id and registry invariants (`test_integration.c` extension).
* **Depends on:** every other package.
* **Acceptance:** §10 (the whole Definition of Done).
* **Budget:** none of its own. I9 enforces §9 and can send a package back with a lever from §11.

---

## 8. Order of work and merge plan

```
G0 (external): GBA memory pass #77 merged -----------------------------------------+
Wave 1  S0 contracts --> S1.M1 tween/beat/camfx + post hooks (shared-file hunks first)|
        H3.M0 prims.c factoring (before custom wave 2)   H2.M0 harness commands       |
Wave 2  S1.M2-M5 | S2 engine (VM, txt, textpack, STRY, verbs, timing, legacy port)   |
        H1.M1 (needs G0 for .bss) | H4.M1 furngen + 60 pieces + room_ref + scene shell|
        S3.M1 authored possessor hook                                                |
Wave 3  S2 content Ch1-2 | S3 content Ch3-4 + combat/pet/exorcism | H1.M2-M3         |
        H2 renderer | H3 renderer | H4.M2 all art, visits, mart, every animation     |
        S4 batch A | S5 batch B (Breach variants after S3's combat milestone)        |
Wave 4  S5 Feast port (after S1.M4 stages) | I9 integration, docs, final gates ------+
```

* **The critical path:** S0 → S1 M1 → S2 engine → S2/S3 content → I9. The house runs in parallel: S0 → H1 M1 → H4 M1 → H2/H3.
* **Merge rule:** each PR is one package milestone and passes CA on its own. A milestone that depends on an unmerged one stays on its branch and rebases. Nobody edits another package's files to unblock themselves; they ask the owner, or the S0 stub covers the gap.
* **How the story stays playable throughout** (SF §15.12): after S2's engine milestone the full playthrough test runs on the legacy port, and each chapter's content replaces its legacy slice.

---

## 9. Global budgets and gates

> **Superseded in part.** The baselines and several gates in this table were stale or unreachable. The re-measured baselines and the binding gates are in §12, tables C-B and C-G.

The baselines are measured, from `custom/design_grow.md` §2.1 and HF/SF, at HEAD `43af439`/`ae10664`. I9 re-measures after G0.

| Resource | Baseline | This SPEC's spend | Gate (CI fails above or below) |
|---|---:|---|---|
| **GBA ROM** (4 MiB padded file = 4,194,304 B) | 2,428,644 B | house art about 819 KB; code about 135 KB; text 172 KB; scripts 86 KB; beats 61 KB; tables, songs, SFX and sprites about 85 KB = **about 1.36 MB** → about 3.79 MB, plus custom about 190 KB → **about 3.98 MB** | ≤ 4,194,304 B hard; a warning above 4,063,232 B. **Levers, in order:** LZ-pack the furniture sprites (about −250 KB); drop back facings; LZ the beat scripts (SF chose not to); as a last resort, an 8 MiB padded file (an I9 decision; flash saves are unaffected) |
| **GBA EWRAM** (`.bss`) | 7,620 B free now; #77 targets ≥ 24 KB | S1 464 + S2 176 + S3 24 + H1 3,408 + H2 ≤ 64 = **≤ 4,136 B** | after merge: free ≥ (the post-G0 figure − 4.2 KB) **and ≥ 8,192 B** |
| **GBA IWRAM** | 6,460 B for the 6 KB stack | **0 B** (affine OBJ, not a span blit; OAM DMA from ROM) | +0 B in `memreport` and `nm` |
| **GBA `gba_big`** (17,408 B) | shared by OW / R3D caches | the house claim ≤ 11.2 KB while the house runs | ≤ 17,408 B per claim |
| **GBA scene scratch** (12,288 B) | — | house ≤ 4 KB (+ the 8 KB tail as `plat_cache`); `OwScratch` +512 B | ≤ 12,288 B |
| **PS1 resident** (below-stack headroom) | 139.2 KB; custom B → 128.7 KB | removed: −73.3 KB (story text and rodata no longer escape into the queue); added: S1 14 + S2 9 + window 36 + H1 1 + H3 8 KB of code and data + `.bss` (`STRY` 96 B + `HOME` 3,408 B + S1 464 B) ≈ **+72 KB** → net **≈ −1 KB** (about neutral) | ≥ 128 KB (131,072 B) below the stack top, ≥ 64 KB stack (`elf2exe.py`) |
| **PS1 overlay slot** (111,376 B) | story group 49.8 KB | story ≤ 75 KB; cutscene ≤ 100 KB; home ≤ 100 KB target (108 KB hard); exorcism, pet ≤ 100 KB; combat ≤ 108 KB | `ovl_plan.py` report per group |
| **PS1 disc** (2 MB budget, task #82) | — | `TEXT.BIN` about 330 KB + ADPCM about 30 KB + the home group about 95 KB in `OVL.BIN` | ≤ 2 MB game disc |
| **Save, story slot** (40,960 B) | 13,504 B | `HOME` 3,408 + `STRY` 96 (+ custom's 8 KB story heap cap) | ≤ 40,960 B, computed |
| **Save, free-play slot** (90,112 B) | 59,148 B; custom: 86.6 or 89.6 KB | `HOME` dynamic under `save_budget_left` | `save_joint_worst_case` ≤ 90,112 B |
| **PS1 memory card** | 2 blocks | typical house about 500 B compressed | 2 blocks for the test house; the 3-block edge case is documented (HF §9.4) |
| **Per-chunk cap** | 8,192 B | `STRY` 96, `HOME` 3,408 | each ≤ 8,192 B |
| **fps: GBA** | — | house steady ≥ 28; rotate ≥ 24; Cellar, siege, modal ≥ 20; cutscenes and summit ≥ 28; Feast pop ≥ 24 | harness `fps` labels |
| **fps: PS1 (Redux)** | — | house steady ≥ 28 (30 on a console); orbit ≥ 22; summit ≥ 28; Feast pop ≥ 22 | harness `fps` labels |
| **CPU per tick** | 559k cycles GBA / 1.13M PS1 | VM 50-200; beat about 50; springs 15 per axis; text decode 15k only when a line opens; palette lerp 5k only during flashbacks and inserts | no regression in the existing heavy-scene fps scripts (tasks #18, #35) |
| **Unit tests** | 337 | +about 110 | `MAX_TESTS` 1024 |

---

## 10. Definition of done (the whole SPEC)

1. Every package's acceptance criteria are met on the merged tree, and GitHub CI (`core`, `gba`, `ps1`) is green.
2. **The story is played end to end** on native, GBA and PS1 by `integration_story_home.txt` / `story.txt`, with the same `story_fingerprint` on all three and the same `L_PUNCH` ticks.
3. **The house works** on all three: build, decorate, rotate, photo, visit a generated home, gift, HomeMart, the haunting, the siege, the Open House. The renderer parity golden holds, and the fps gates hold.
4. **The timing contract is enforced by tests, not by review alone:**
   * `beat_script_timing` (≥ 75% of gaps on the ladder; inserts 3-4 frames; a button 12-25 frames after `L_PUNCH` or an explicit `NOBUTTON`; one SFX per visual op; one expression per line);
   * `silence_budget` (20-25% per chapter; Interlude III exempt);
   * `story_style_rations` (≤ 1 `{d}` per conversation; < 80 in the whole game; `NOBUTTON` ≤ 50% of sincere beats per chapter; "Ten years I" exactly 4 times; ≤ 4 news-ticker "Later"s);
   * `story_length_budget` (each chapter within ±10% of SF §8; side content ≥ 12 h declared).
5. **Content counts:** 27 episodes + 8 specials + P0 + E1-E2; 109 live missions (118 rows); 49 old quests re-timed; 9 new songs; catalog 306-330 with every SF reward; 74 or more story furniture rewards granted.
6. **Safety rules** (SF §3, §0): no flirting with any `KID` or teen, checked by a test over all Grumbleguts lines' targets; the Feast outcome conforms to R15; full-screen flashes ≤ 3 Hz; reduced-motion options keep timing.
7. Docs updated (I9); stubs removed; no `STUB:` in any script; R1-R22 reflected in `docs/STORY_ENGINE.md`, `docs/TIMING.md` and `docs/HOUSING.md`.

---

## 11. Risks

| # | Risk | Mitigation | Owner |
|---|---|---|---|
| 1 | The GBA ROM goes over 4 MiB once the customisation lands | the §9 levers in order; I9 decides on an 8 MiB pad | H4, I9 |
| 2 | G0 slips and EWRAM is too tight for `HOME` | the HF §6.5 fallbacks (about −600 B); H1 M1 can land behind a build flag that keeps `HOME` out of the GBA build until G0 | H1, I9 |
| 3 | The save goes over the free-play slot when combined with the customisation | R13's joint budget + `save_joint_worst_case` | S0, H1, custom |
| 4 | The PS1 house is too slow with the 8 KB cache | culling by distance during an orbit; tiny pieces become billboards; the floor and walls are always cached | H3 |
| 5 | The PS1 `home` group goes over its cap | shared description phrases (−6 KB); move `home_defend` tables into the pack | H3, H4 |
| 6 | Timing drifts back into frame constants | the lint test; the review rule "no literal frame counts in `home_*.c` / `story_*.c` outside tables"; trailer parity tests | S1, I9 |
| 7 | 59,000 words of content arrive late | the legacy port keeps the game playable; chains are independent; the packs and lint let writers work in parallel | S2-S5 |
| 8 | Two packages need the same file | §6.3 hunks; otherwise the owner does the work on request; the S0 stubs bridge gaps | I9 |
| 9 | The PS1 resident window (36 KB) squeezes the 128 KB gate if custom also spends | measured after each merge; levers: shrink the side slots to 1 × 8 KB (−8 KB), and the episode slot to 10 KB by splitting Ep 4.6 into two packs | S2 |
| 10 | Long "Later" and wrongness tables make reloads fire everything | `STRY.gag_cool` cooldowns are saved (SF §17.2) | S1, S2 |

---

## Appendix A: which package owns each mission and episode

| Package | Main path | Side missions |
|---|---|---|
| S2 | P0; Ep 1.1-1.6; Interlude I; Ep 2.1-2.7 (incl. the Bottle); F1; D1; Ch1-2 common packs; W1-W16; NG+ (Ch1-2) | — |
| S3 | Ep 3.1-3.7; F2; F3; Interlude III; Ep 4.1-4.7; D2; F4; E1; E2; title variants; credits; Say It pool; NG+ (Ch3-4) | — |
| S4 | — | **A** S100 S101 S102 · **B** S103 S104 S105 S202 S203 S106 · **C** S196 S107 S108 S197 S198 S109 · **D** S199 S110 S111 S112 S113 · **S** S114 S115 S116 · **E** S117 S118 S201 S119 S120 · **F** S121 S122 S123 · **G** S124 S125 S126 · **H** S127 S200 S128 S129 · **I** S130 S131 S132 S133 · **J** S134 S135 S204 S136 · stand-alones S143 S144 S145 S146 S147 S148 S149 S150 = **54**; plus quests #0-#48 re-timed. E3 = S143. |
| S5 | — | **K** S137 S138 S139 S140 · **M** S151 S152 S153 · **N** S154 S155 S156 · **O** S157 S158 S159 · **P** S142 S161 S162 · **Q** S163 S164 S165 · **R** S166 S167 S205 S168 S206 · **L** S169 S170 S171 · **U** S172 S173 S174 · **T** S183 S184 S207 S185 S186 · **V** S208 S209 S210 · stand-alones S176 S178 S179 S180 S181 S182 S187 S191 S192 S195 S211 S212 S213 S214 S215 S216 S217 = **55**. E4 = S186. |

Total: 54 + 55 = **109**. The retired ids S141, S160, S175, S177, S188, S189, S190, S193 and S194 are `KIND_NONE` rows, which S5's table test checks.

**Missions that wait for another package** (the writer may author them, but not merge them, before the dependency lands):

| Missions | Wait for |
|---|---|
| S171, S174, S178-S181 | S3's Breach and Arena variants |
| S168, S205, S206 | S3's Ep 4.4 (Wisp in a body) |
| S186 | S1 M4 (stages) and S3's exorcism skins |
| Every housing-verb mission (34) | H1 M1 |
| S138 (closet), S166 (`placed by: ???`) | H1 M3 (haunt / gift tags) |

---

## Appendix B: which package owns each test and runner script

| Owner | Unit tests | Runner scripts | Platform scripts |
|---|---|---|---|
| S0 | `test_contracts.c` | — | — |
| S1 | `test_tween.c` (incl. `tween_matches_trailer`), `test_beat.c`, `test_camfx.c`, `test_dialog_codes.c` | `timing_showcase.txt`; re-approves `story_cutscenes.txt` | GBA `fps cutscene_wall` (inside S2's `story_ch1.txt`) |
| S2 | `test_story.c` (kept tests), `test_story_vm.c`, `test_textpack.c`, `test_story_lint.c` (box fit, timing lint, silence, rations, length) | `story_p0.txt`, `story_ch1.txt`, `story_int1.txt`, `story_ch2.txt` | `gba/harness/scripts/story_ch1.txt` |
| S3 | `test_story_ch34.c`, `test_exorcism_authored.c` | `story_ch3.txt`, `story_invasion.txt`, `story_int3.txt`, `story_ch4.txt`, `story_epilogue.txt` | `gba/harness/scripts/story_ch4.txt` |
| S4 | `test_side_a.c` | `side_sample_a.txt` | — |
| S5 | `test_side_b.c`, `test_side_table.c`, `test_feast.c` | `side_sample_b.txt`, `side_chains.txt`, `story_feast_a.txt`, `story_feast_b.txt` | GBA `fps feast_pop` (in `story_ch4` replay or its own script) |
| H1 | `test_home.c`, `test_home_defend.c` | — | — |
| H2 | — | — | `gba/harness/scripts/home.txt` |
| H3 | — | — | `ps1/harness/scripts/home.txt` |
| H4 | `test_home_visit.c`, `test_furngen.c`, `test_home_render.c` | `home_tutorial`, `home_editor`, `home_rooms`, `home_visit`, `home_haunt`, `home_defend`, `home_mart`, `home_gallery` (.txt) | — |
| I9 | `test_integration_story_home.c` | `integration_story_home.txt` | `ps1/harness/scripts/story.txt`; the fingerprint and `L_PUNCH` parity checks |

---

## Appendix C: checks every content author runs (S2-S5)

Run `make -C core gen && make -C core test` locally; `textpack.py` and `test_story_lint.c` enforce most of these:

1. **Every scene** has: a camera move or cut; one expression change per line; one SFX per visual beat; one silence; and a button 12-25 frames after `L_PUNCH`, or an explicit `[NO BUTTON]` / `[NO GAG]`.
2. **Gaps** sit on the ladder 2/3/5/8/13/21/26 (±1). Inserts are 3-4 frames and make one persistent change. In a rule of three, the intervals are equal (15-16 frames), every channel escalates, and the topper comes R13 later.
3. **Rations:**
   * ≤ 1 `{d}` per conversation; GURGLE lines are plain text;
   * `NOBUTTON` on ≤ half of the sincere (`HEART`) beats per chapter, and never twice in a row in a chain;
   * CUT_WALL_FALLS and the summit are never undercut;
   * "Ten years I X because Y" only in Tallyworn Ep 4.1, Haze S139, Dirgelet S179 and Mopsworth S187.
4. **Density:** 1 punchline per 3-6 lines; 0 gags for 3-5 s after a heartfelt beat; 5-15 s of free control after every big beat.
5. **Every mission:** a "Later" 1-10 minutes after turn-in. Only S140, S156, S168 and S191 may use the news ticker.
6. **The box:** 3 × 28 at 240×160, 4 lines at 320×240; `/` and `//` used.
7. **Canon:** pronouns as SF §0; kids are off limits for any flirting; Grumbleguts is never mean, and the joke is his vanity and gluttony; nothing mocks real people; cute characters are never shown harmed in the cute half of a whiplash cut.
8. **Determinism:** every pick goes through `story_rng(ctx)` with a unique `ctx`; no wall clock.
9. **Length:** declare `minutes=` honestly. The chapter sums must land within ±10% of SF §8.

---

## 12. Critique fixes (binding; wins over §1-§11 and Appendices A-C)

This section is an adversarial review of v1 against the tree, done read-only on 2026-10-18. Nothing in the repo was edited, built or git-touched. Measurements came from the build outputs already in the tree (`gba/build/doomogachi.map`, run through `gba/tools/memreport.py` with `python3 -I` from `/tmp`; `ps1/build/doomogachi.ldmap` and `ps1/build/ovl.ld`, both built 2026-10-18 at about 11:04) and from the sources and docs. Each fix is numbered C1-C25. Where a fix changes a package's scope, the package text in §7 is to be read with the fix applied.

### C-B. Baselines re-measured (v1's numbers were stale)

| Resource | v1 said | Measured now | Source |
|---|---:|---:|---|
| GBA ROM | 2,428,644 B | **2,500,724 B** | memreport on `gba/build/doomogachi.map` |
| GBA EWRAM free | 7,620 B | **8,264 B** | memreport (253,880 / 262,144 used) |
| GBA IWRAM left for the 6 KB main stack | 6,460 B | **6,320 B** (176 B spare) | memreport |
| GBA OBJ VRAM `0x06014000` (16 KB) | "free" (HF §6.3) | **used**: r3d's texture cache, 4 × 4 KB | `gba/src/r3d.c:347-353` (`VTEX_BASE`) |
| PS1 below the stack top (`0x801FFFF0`) | 139.2 KB | **86,016 B (84 KB)**: the image ends at `0x801EAFF0` | `doomogachi.ldmap` (`__ovl_slot_end`); `elf2exe.py` only enforces 64 KB |
| PS1 overlay slot | "111,376 B, fixed" | **not fixed**: `OVERLAY` = the biggest group rounded up to 2,048 B. Today combat is 118,896 B, so the slot is **120,832 B** | `ovl_plan.py:390-406`, `ovl.ld`, ldmap |
| PS1 story group | 49.8 KB | **38,076 B** (cutscenes are their own group: 71,972 B) | ldmap `.ovl_14` / `.ovl_04` |
| PS1 other groups | — | fishcook 93,052 · minigame 88,308 · freeplay 71,988 · library 65,308 · concert 52,272 · creator 51,972 · studio 45,524 · island 41,232 · dream 38,692 · exorcism 28,756 · pet 21,776 · workshop 14,844 · gallery 5,096 | ldmap |
| PS1 resident story text + rodata that the VM can remove | 73.3 KB | **59,076 B**: `story_script` 29,268 + `story_side` 18,964 + `story_talk` 10,844 | ldmap per object, alloc sections only |
| SFX PCM | "+8 KB fine" | **47,793 of 49,152 B used (1,359 B free)**; `test_audio.c:161` asserts the cap | `core/gen/sfx_data.c`, `audio.h:86` |
| Unit tests | 337 / 512 | 337 / 512 ✓ | `test_main.c:6` |
| GBA fps of story-class scenes | assumed ≥ 28 | dialogue, exorcism, pet, concert **6.6-8.9**; 3D including the overworld **8.7-12.9**; shop and roster 21-27; apartments 30 | `docs/GBA.md` speed pass 3 |
| PS1 fps (Redux) | — | dialogue **21-22**, story overworld 29.3, exorcism 26.7, citizen close-up (370-550 polys) 24-30 with 3D at 17-28 ms | `docs/PS1.md` §12 |

**C1. Baselines.** All §9 baselines are replaced by table C-B. I9 measures them again at G0, and every gate below is relative to that G0 measurement.

### PS1 RAM and overlays

**C2. GBA paths and harness commands.**
* GBA scripts live in `gba/harness/scripts/`; `gba/scripts/` does not exist. This was fixed in place above.
* `gba/Makefile` lists its scripts explicitly (`test-game`, `test-coresave`). After G0, H2 owns the new `test-home` / `test-story` targets. S2 and S3 send their script lines to H2.
* The GBA harness already has `scene` and `push` (`harness.c:544`, `hcmd.c` `GBA_HC_SCENE/PUSH`), and so does the PS1 harness (`ps1/src/hcmd.c:114`). The `integration_todo` item is stale. R20 therefore shrinks to `seed`, `expect-flag`, `expect-text` and `story-fp` on all three runners.

**C3. The PS1 link set.**
* The problem:
  * `ps1/CMakeLists.txt:40` globs every `core/gen/*.c` into the EXE.
  * `ovl_plan.group_of()` returns `None` (resident) for anything outside `/core/src/`.
  * So `core/gen/text_*.c` (about 320 KB of text, scripts and beats) would be linked resident on the PS1 as well as streamed from `TEXT.BIN`. So would `furn_data.c` with the GBA's 0.8 MB of iso sprites. Either one overflows 2 MB of RAM.
* The fixes:
  * **S2 adds a hunk to `ps1/CMakeLists.txt`.** It excludes `/core/gen/text_[a-z0-9_]+\.c$`, adds a custom command that runs `textpack.py` and writes `TEXT.BIN` into the build directory (never committed), and adds one `<file name="TEXT.BIN" …/>` line to **`ps1/iso.xml`**. That file is the disc manifest; there is no `ps1/iso/` directory.
  * **`furngen` emits three files:**
    * `core/gen/furn_grant.c`: the `{unlock → furn}` table and toast names, ≤ 2 KB, resident;
    * `core/gen/furn_data.c`: catalog, prims and strings, in the `home` group;
    * `core/gen/furn_iso.c`: the GBA span sprites; only GBA and native link it, and the PS1 CMake filter excludes it.
  * **H3 extends `group_of()`** so that `/core/gen/furn_data` and `/ps1/src/room` belong to the `home` group. The R4 exclusions stay.
  * **`ovl_plan.py` gets a check:** if any section of the `home` or `story` groups is reachable from resident code, the build fails and prints the edge. This makes C6 enforceable.

**C4. The slot is the biggest group.**
* **The rule.** No group may grow past 118,896 B (today's combat) without an I9 decision. Every KB above it moves the slot, and the resident headroom drops by the same amount.
* **Group caps (these replace the caps in §7 and §9):**

  | Group | Cap |
  |---|---|
  | story | ≤ 75 KB |
  | cutscene | ≤ 100 KB |
  | home | ≤ 100 KB, and ≤ slot − 14 KB = **104,448 B** for C5's tail window |
  | exorcism | ≤ 64 KB (28 KB today) |
  | pet | ≤ 64 KB (21 KB today) |
  | combat | **net growth ≤ +1,936 B** (the sector slack) |

* **Combat.** v1's "combat ≤ 108 KB" was already broken by 10 KB before any work started.
  * S3 puts boss captions in packs (episode slot) or in the stash (C5).
  * S3 puts Grumbleguts' mechanics, the photo verb and the Vapor Sample in a new file `combat_story.c`, and the Breach entries and variants in `levels_story.c`. Both names match the combat group's regex.
  * Any growth is offset inside the group.
  * If that is impossible, I9 chooses one of two options:
    * (a) split `levels_select$` into its own group, accepted only if `ovl_plan` shows ≤ 2 KB of `levels.c` turning resident;
    * (b) a larger slot, paid from the C5 ledger.

**C5. The PS1 resident ledger.** This replaces the §9 PS1 row and the size of the R2 window.
* **The text window shrinks from 36 KB to 14 KB resident:** the episode slot (12 KB) and the directory (2 KB).
  * **The side-pack slot (8 KB) and the chapter-common slot (6 KB) move to the last 14 KB of the overlay slot.** H3 adds a hunk to `ps1/src/overlay.c` (`ovl_tail(bytes)`).
  * Loading a group that ends at or below slot − 14 KB keeps the tail intact. Today that is every group except combat.
  * The loader marks the tail invalid after a larger load. The VM then re-reads its live pack ids on return, which is one read of about 0.1 s at a boundary that already exists.
* **The caption stash.** Before a side mission's `PUSH combat`, the VM decodes the ≤ 4 lines that scene needs into a resident stash of 4 × 64 B (`txt_stash`), and the combat captions read the stash. The lines are declared as `push combat … captions=a,b,c`; `textpack.py` rejects a side `push combat` without the `captions=` list.
* **The Feast pack** (≤ 10 KB) loads into the episode slot. It runs post-game, when no episode is live. v1 put it in an 8 KB side slot, where it did not fit.
* **S1's NPC/camera scripting and the spawner** go in `overworld_*.c` files, which are in the story group. `ow_spawner.c` is renamed `overworld_spawn.c`; under the old name it would have been resident by accident. S1's resident code drops from 14 KB to about 10.5 KB.
* **The ledger** (PS1 resident; every package states its line, and I9 re-measures after each merge):

| Added | KB | Removed | KB |
|---|---:|---|---:|
| S1 core (tween 2, beat 3, camfx 2.5, audio_fx 1.5, dialogue codes 1.5) | 10.5 | `story_script.c` text + rodata | 29.3 |
| S1 PS1 platform (`postfx.c`, `xform.c`) | 3.0 | `story_side.c` | 19.0 |
| S1 tables, shake profiles, palettes | 2.8 | `story_talk.c` (its small-talk generator stays resident: free play calls it) | 4-10.8 |
| S1 synth SFX strings (C11) | ≤ 2.0 | | |
| S2 `scriptvm.c` + `txt.c` (including the 1.6 KB Huffman table) | 9.0 | | |
| S2 window (episode slot + directory) and stash | 14.25 | | |
| generated `side_table`, `possess_table`, `furn_grant` | 4.5 | | |
| 9 new songs' MML (the `audio_songs` table is resident) | 5.5 | | |
| H1 `house_api.c` | 1.0 | | |
| H3 `prims.c` (`room.c` goes to the home group, C3) | 3.0 | | |
| `.bss`: `STRY` 96 B + `HOME` 3,432 B + S1 208 B | 3.7 | | |
| S3 `story_sprites` code shared across groups (C24) | ≤ 4.0 | | |
| **Total** | **≈ 63** | **Total** | **52-59** |

* **The net** is +4 to +11 KB, so 84 KB becomes about 73-80 KB.
* **Gates:**
  * after every merge, the headroom is ≥ 72 KB and ≥ the G0 figure − 12 KB;
  * the window lands **in the same PR** that removes the resident strings;
  * customisation work spends from the same ledger, and I9 owns the joint total.
* **Task #79** (≥ 128 KB) cannot be reached by this SPEC; it is out of scope and is stated as such in `docs/PS1.md`.
* **PS1 levers, in order:**
  1. move the callbacks of `story_places.c` (6.2 KB resident) into the VM: about −5 KB;
  2. split Ep 4.6 so the episode slot is 10 KB: −2 KB;
  3. story-only songs (the 9 new ones) play from a 2 KB resident song buffer filled from their pack, instead of resident MML: −3.5 KB;
  4. compile `tween`/`beat`/`camfx` with `-Os`: about −1.5 KB.

**C6. Resident code never calls overlay code.**
* **Why.** On the PS1, `scriptvm.c` and `beat.c` are resident. A direct call into the story or home group pulls that section resident. Taking a function's address does the same (`ovl_plan` treats escaping addresses as resident).
* **Ops returned to the host.** `vm_tick()` returns **`VM_EXT`** with the op for these, and the host scene handles it in its own group code:
  * VM ops: `CAST`, `NPC`, `CAM`, `TELEPORT`, quest UI, `INSERT` drawing, `FREEZE_CAP` drawing, and the `HOME` sub-ops that need catalog, score or layout code;
  * beat events: `BOP_NPC`, `BOP_CAM`, `BOP_SHOT`, `BOP_HEADTURN`;
  * the host scenes: the overworld, `SCENE_HOME`, `SCENE_CUTSCENE`, `SCENE_STORY_MG`, and combat for captions.
* **`beat_tick()`** returns host-kind events the same way.
* **No function pointers into groups. S1 provides** `ownpc_cast(id, spot, flags)` and the NPC acts in `overworld_npc.c`. Those are called from the story group.
* **`house_api.c` only reads and writes `HOME` `.bss` fields.** Anything that needs catalog, score or layout code runs in the home group and caches its result in `HOME` (C9). This covers `home_grade`, `home_stat`, `home_goal_check`, `home_decorate_for` and `home_defend_result`.
* The C3 check in `ovl_plan.py` enforces all of this.

### GBA renderer and fps

**C7. The GBA isometric cost model, corrected.**
1. **The blit path.**
   * `ow_sprite()` takes an 8bpp rectangular `DoomoImage` (`ow.iwram.c:107`). No span blitter exists in IWRAM, and none may be added: 176 B are spare and task #78 forbids it. HF's "span-skipping sprite path" does not exist.
   * `furn_iso.c` stores span-encoded facings plus a u16 row-offset table, and two existing ARM paths draw them:
     * **(a) plain pieces** (no marker paint, light level 0, no tint): each opaque run is copied with `ow_out()` (ARM in IWRAM, `ow_loops.s`; even length, halfword aligned). THUMB writes the ≤ 2 odd edge pixels with a read-modify-write. About 2-3 cycles per pixel.
     * **(b) recoloured, lit, tinted or flipped pieces:** the rows that overlap the rect are decoded into a 64×64 **staging image in the house's `gba_big` claim**. `memcpy`/`memset` are IWRAM ARM, so this costs about 2 c/px, and the decode applies the flip. Then the `ow_sprite` fast path draws it at about 7-8 c/px. A tint or night forces `ow_sprite`'s slow path, about 11 c/px (the Cellar).
   * The claim becomes 11,136 + 4,096 + 256 (larger dirty lists, below) = **15,488 B ≤ 17,408** ✓.
2. **Frame costs** (559k cycles = one 33.3 ms tick):

| Case | Work | Cycles | Result |
|---|---|---:|---|
| Full recompose 6x6, 20 pieces (half of them recoloured) | bg 40k + walls 36k + floor 28k + 8k px × 2.5 + 8k px × 10 + sort and LUT 10k | 214k (+ HUD 15k + logic 40-80k) | 16-19 ms, 30 fps |
| Full recompose 8x8 Cellar, 40 pieces, tinted | 136k + 32k px × 13 + 10k | 562k + logic | 37-40 ms, 15 fps on **at most 2** recompose frames |
| Steady: 4 chibis + 3 animated pieces | 3.9k px per page × (3 + 1.5 × 8) × 2 pages + chibis (cached `bodyspr_image`) 12k + HUD 15k + logic | 185-225k | 11-13.5 ms, 30 fps |
| Hold the Door wave 3 | stays on dirty rects (cap 20 per page, about 15 used) instead of recomposing every frame: 12.6k px × 11 × 2 + logic 80k + HUD | about 370k | 22 ms; overflow → recompose about 400k = 24 ms |
| Talk close-up | the hero chibi re-renders in slices (about 12 ms per tick, measured) + `ui_dialog` | | gated against the story's own dialogue baseline (C22) |

3. **The opaque HUD.**
   * The two 12 px bars register with `plat_room_ui_opaque(x, y, w, h)` and are left out of HF §6.4.3's UI-bbox feedback.
   * Without that, about 40k cycles a frame go to redrawing the room under the HUD.
4. **OBJ VRAM is r3d's.**
   * Add `gba_objvram_claim(owner)` as a hunk in `gba/src/r3d.c`, which belongs to the GBA platform (tasks #18/#35). H2 asks for it.
   * When r3d loses the claim it clears `vtex_id[]` and re-uploads once (≤ 16 KB).
   * S1's `obj.c` claims OBJ VRAM only on frames without a `plat_view3d`. Inside a 3D view, `plat_image_xform` falls back to software: no squash, unchanged timing.
5. **OBJ VRAM in the house:**

   | Use | KB |
   |---|---:|
   | the held ghost | 4 |
   | the cursor | 0.5 |
   | 4 emotes | 2 |
   | FX | 2 |
   | 2 hero-squash slots | 3 |
   | **Total** | **11.5 of 16** |

   v1 had 10.5 (HF) + 4 × 1.5 (S1) = 16.5 KB, which overflowed.
6. **OBJ always draws over the mode-4 bitmap.** BG2 is opaque, because the game never leaves index 0 as BG transparency. A squashed hero OBJ would therefore cover the dialogue box, the HUD and any furniture in front of it.
   * `plat_image_xform` uses OBJ only during the squash itself (≤ 12 frames), and only if no later 2D draw in that frame intersects the OBJ's bbox.
   * Otherwise it draws unsquashed in the bitmap.
   * The per-frame 2D bbox accumulator therefore moves from H2 to S1's `gba/src/postfx.c`, shared by everyone.
7. **The flip is `video_flip()` in `gba/src/main.c:204`, not `video.c`.** S1's hunk (OAM DMA, BG2X/Y and PA-PD, BLDY, palette DMA after the vblank wait) goes there. `video.c:62` keeps only the `DCNT_OBJ` bit.
8. **`plat_save_scratch()` takes all of `gba_big` during a save.**
   * On any `gba_big` claim change, `obj.c` drops back to its own 32-entry shadow.
   * The house re-registers its shadow after rebuilding, and the first frame after a save is a full recompose.

**C8.** The fps gates are replaced by table C-G (C22) on both consoles.

### Save and memory math

**C9. Save and memory.**
* **`HOME`.** Two additions bring the worst case to **3,432 B**:
  * a pending-gag queue (4 × u16 = 8 B), because §5's `later { prop house … }` had nowhere to live;
  * cached results (16 B): 5 stats, 9 room grades, the `decorate_for` result and the defence result, which `house_api.c` serves on the PS1 without the home group (C6).
  * The 8,192 B per-chunk cap and `DOOMO_SAVE_EXT_MAX` staging both hold ✓.
* **`STRY`.**
  * The **LATER queue** (4 × {u16 script, u16 due game-minute, low 16 bits} = 16 B) takes the 16 spare bytes. STRY stays at exactly 96 B with no spare left, and the next field needs STRY v2.
* **R13's budget.**
  * Raw sizes are not an upper bound. The LZ in `save.c` writes one control byte per ≤ 128 literals (+0.79% worst case), plus the 48 B header and 10 B per chunk header.
  * **The formula:**
    > `save_budget_left = cap − (last_total(slot) + Σ ceil((size_now − size_at_last_io) × 129 / 128) + 10 × chunks_new) − 2,048`
  * The terms:
    * `last_total` is header field 8 (the total file size), recorded by `save_budget.c` from the `save_ext` write/read path (S0 owns `save_ext.c`), so `save.c` (custom) is not edited;
    * `size_at_last_io` comes from the same callbacks.
  * It never runs the encoder: a counting pass over a 60-90 KB body for every placement is far too slow on the GBA.
  * `home_place_gift` and other growth outside the editor are checked too. A gift that does not fit goes to storage.
* **The story slot:** 13,504 + 3,432 + 96 + the custom story heap cap of 8,192 = 25,224 ≤ 40,960 ✓.
* **GBA EWRAM:** S1 464 + S2 176 + S3 24 + H1 3,432 + H2 64 = **4,160 B**.
  * Today's 8,264 free would leave 4,104, which is under the 8,192 gate.
  * **H1's `HOME` `.bss` lands only once the post-G0 figure is ≥ 12,352 B free** (#77 targets 24 KB).
* **The scene scratch.**
  * v1's house split (H1 2.5 + H4 1.5 + 8 KB cache) is exactly 12,288 B, with no margin.
  * `plat_cache_cap` = 12,288 − the house's measured use rounded up to 256, computed in `enter()`.
  * The GBA passes 0, because its caches are in `gba_big`.
* **`ow_scratch_ext` is a cache.**
  * Every scratch claim destroys it:
    * `grumbleguts_gfx.c:832` claims the whole 12 KB for `CUT_FEAST_POP`;
    * cooking (FCOK), the pet (PETS), welding (WELD) and the house also claim it.
  * The Feast's guests are rebuilt from (`STRY.feast_seed`, the friends list) in the "had to reclaim" path of `ow_scratch_claim()`. Its progress lives in `STRY.feast_*` and in the VM's resident vars. The Say It walk is the same.
* **The house's undo ring, selection and clipboard** are in the scratch. They are lost when the house pushes the Workshop bench (which claims WELD), and the history clearing on any push is documented.

### Ownership and hand-offs

**C10. Collisions with the customisation candidates that v1 missed.** Both `custom/design_packed.md` and `custom/design_grow.md` name call sites in files this SPEC gives to S1, S2, S3, H2 or H3.
* **The rule.** The custom spec owns each option's definition and getter. The read in a file owned here is inserted **by this SPEC's owner**, as one marked hunk, at the custom owner's request, in the order below.

| File | Custom call sites (both candidates) | Owner here | Order and hand-off |
|---|---|---|---|
| `ui_dialog.c` | text speed, hold to skip, word filter in `copy_str`, confirms, subtitles | S1 | S1 M3 lands first, then up to 5 requested hunks. The filter runs on the decoded line before beat codes are parsed. |
| `cutscene.c` | skip seen cutscenes | S1 | read inside M4's whole-cutscene `START` skip |
| `overworld.c`, `overworld_npc.c` | quest markers, autosave, palette rot, thought bubbles, footsteps | S1 | after M5 |
| `story.c`, `story_script.c`, `story_talk.c`, `story_questlog.c` | NG+ carry-over, `%P`, markers, pacing (`script_ambient_talked`), encounters (`script_nest_encounter`) | S2 | `story_script.c` is gutted, so those hook sites disappear. S2 exposes `story_pacing_scale()` and `story_encounter_rate()`, which the VM-side ambient and nest code calls, and the custom design re-targets to them. |
| `dream.c` | dream rate, nightmares | S2 | |
| `exorcism*.c` | difficulty, phases, fails, clues | S3 | Authored possessors honour `EXO_*`. The scripted fails (Wheezle in Ch2, the Sprinklepop rite) ignore difficulty, and this is documented. |
| `pet*.c` | about 10 options | S3 | |
| `combat.c`, `combat_hud.c`, `combat_scene.c`, `actor.c`, `levels*.c`, `arena_modes.c` | about 15 options | **the custom spec keeps `combat*.c` and `actor.c`** | S3's work moves to `combat_story.c` and `levels_story.c` (C4), with ≤ 3 hook hunks: on kill, the HUD caption slot, the level table |
| `gba/src/main.c` | `read_pad` (L/R swap, sleep) | S1 hunk first (`video_flip`), then the custom spec | |
| `gba/src/hcmd.c`, `ps1/src/hcmd.c`, `gba/harness/harness.c`, `ps1/harness/run.py` | `fp`, `fpgold` | H2 / H3 | separate hunks; the names are disjoint (`seed`, `expect-flag`, `expect-text`, `story-fp`) |
| `ps1/src/spu.c`, `gba/src/audio.c` | SFX to the SPU | custom | S1 adds nothing there (C11) |
| `gba/tools/memreport.py`, `ps1/tools/elf2exe.py` | budget gates | G0 / custom | I9 asks the owners for its gate lines |
| `ps1/CMakeLists.txt`, `ps1/iso.xml` | — | S2 hunks (C3) | |
| `ps1/src/overlay.c` | — | H3 hunk (C5 tail) | |
| `gba/Makefile` | — | G0, then H2 (C2) | |
| `core/Makefile` | — | foundation; S2 (`textpack`) and H4 (`furngen`) add one rule each | |
| `gba/src/r3d.c` | — | GBA platform (#18/#35) | C7 claim hunk, on H2's request |
| `core/src/fp_graveyard.c` | — | free play / custom | one clause in `fp_ghost_rite_blocker()` (C13), on S5's request |
| `core/src/maps.c`, `core/include/doomo/map.h`, `core/data/maps/` | — | action / levels | S3 adds the Ninth Cloud remix map file and one registry line |
| `cook_scene.c`, `concert_scene.c`, `fish_scene.c`, `studio*.c`, `weld_game.c`, `mg_whack.c` | — | their teams | no edits (C12) |
| `core/tests/test_integration.c` | — | integration | I9 writes only its own new file. The existing loops already cover new registries. |

**C11. The SFX budget.**
* `test_audio.c:161` fails above 49,152 B of PCM, and 47,793 B are already used. The generator is `tools/sfxgen.py` at the repo root.
* v1's "GBA ≤ 8 KB" breaks that test and adds about 8 KB resident on the PS1.
* **The fix:**
  * **The 15 new comedy SFX** (15 ids, not "13"), the pluck and GURGLE's voice are **synth one-shots**: short MML strings played through `audio_sfx_synth(id)` in S1's `audio_fx.c`, plus S1's one mixer hunk in `audio.c`. That is ≤ 2 KB of strings and 0 B of PCM.
  * Each one borrows tone channel 1 or 2 for ≤ 12 frames, like the existing voice blip (`audio.h:98`).
  * Only `SFX_SCRATCH` may be PCM: ≤ 1,300 B through `sfxgen.py`, with the audio owner's sign-off.

**C12. Variants of other modules' scenes.**
* **The problem.** Side missions ask for changes inside scenes no package here owns:
  * cooking (S109, S110, S112, S113, S128, S168, S183, S184);
  * concerts (S129, S133, S140's beat-synced bubbles, S156's laugh track);
  * fishing (S144, S191);
  * the Studio (S148, S179);
  * the welding rig (S140);
  * a switched-off Whack.
* **The rule.**
  * A `push` may only use the arg encodings those scenes already document (`scenes.h`, `mg.h`, `concert_int.h`, `fishcook_int.h`, `levels_int.h`).
  * S2 maintains `core/data/story/push_args.txt`, and `textpack.py` rejects any arg outside it.
  * Anything else (triple speed with three grannies, two burners, vanishing ingredients, a laugh-track concert, synced bubbles, the switched-off Whack) is either:
    * **restaged as a story-side minigame** in `story_mg_a.c` / `story_mg_b.c`, reusing that module's art through its public draw API; or
    * **requested as one hunk from the module's owner** before the mission is written.
* **S183's "belly's verdict"** reads var 0 (the cooking grade) after the push and is plain dialogue.

**C13. Grumbleguts is a real citizen.**
* He is roster resident **729** (`roster.h:65`), and the sim already dates and marries him (`sim_social.c:123,164`). R22's shadow friendship, romance and `RES_DEAD` in `STRY` would be a second source of truth.
* **The fix:**
  * **Friendship and romance** go through the existing sim calls (`BEFRIEND`).
  * **Death** takes the same burial path sim deaths use, which writes `DoomoGrave` and `RES_DEAD`.
  * **`STRY.grumble` keeps only story-only state:** dishes fed, pacifist, exes met.
  * **The no-rite lock** is `SF_GRUMBLE_NO_RITE`, read in `fp_ghost_rite_blocker()` (`fp_graveyard.c:234`) through one clause the owner inserts, or through a weak `story_rite_blocker(id)`.
  * **Kids stay off limits** through the existing romance age gate plus S5's test over all of his lines.

### Opcodes, features and their owners

**C14. Gaps in opcodes and features.**
* **`[DAY n]` / `[NIGHT]` had no op.** Add the VM op `TIME` (a = `SET_NIGHT` / `SET_DAY` / `ADVANCE`, b = n), owned by S2. It uses the existing clock API, and the sim's minute tick applies as normal.
* **Furniture rewards.**
  * `REWARD` carries only an item and an unlock, so add the `HOME` sub-op **`GRANT`** (furniture id → `home_grant`). Trophies use it too (R8).
  * `later { prop house X … }` becomes the `HOME` sub-op **`PROP_GAG`**, which queues into `HOME.pending_gag` (C9).
* **`VISIT`** is not a resident call. It is `PUSH home arg=2+id`, owned by H4.
* **`LATER`**
  * Pending callbacks are stored in `STRY.later[4]` (C9).
  * A callback fires on the game clock (never the wall clock), at most one per minute tick, and shares the spawner's cooldown.
  * A fifth pending callback fires at once, when its mission is handed in.
* **Side minigames had no host scene.**
  * S0 declares `SCENE_STORY_MG` and `SCENE_STORY_TIMING` (`scenes.h` plus weak stubs).
  * S2 owns their shells in `story_verbs.c` / `story_timing.c`, plus a `story_mg_table` registry that `story_mg_a.c` (S4) and `story_mg_b.c` (S5) fill through weak pointers. The registry is read through a volatile pointer and checked in `test_contracts.c`.
  * `scenes.h`'s maintainer becomes I9 (add-only), not H4.
* **Notation codes used in `story_final.md` that §5 did not map.** The mappings:
  * `[SILENCE]` → `MUTE` until the next `MUSIC`;
  * `[WAIT A]` → `WAIT_A`;
  * expression codes (`[SAD]`, `[SHOCK]`, `[HAPPY]`, `[LOVE]`, `[SICK]`, `[SLEEPY]`, `[ANGRY]`, `[NEUTRAL]`, `[EXPR x]`) → `BOP_EXPR`, or `SAY_X` outside beats;
  * `[BRAKE]` and `[ZOOM]` → `CAM`;
  * `[WOBBLE]`, `[JELLY]`, `[BOING]`, `[SQUASH a/b]` and `[POP TW_x]` → `BOP_POP` with a tween id;
  * `[FREEZE ALL]` → `BOP_FREEZE`(all);
  * SFX names like `[TINK]` → `BOP_SFX`;
  * `[COVER n]` → `BOP_CUT` with the cover flag (S1's stage);
  * `[!RIPPLE]` → `BOP_NPC` act `RIPPLE`, which S1 M5 adds: neighbours emote in a chain, 3 frames apart.
  * The list is closed (`docs/STORY_ENGINE.md`), and any other bracket code fails `textpack.py`.
* **E1 and E2 had no beat ids.**
  * Append `BEAT_E1` and `BEAT_E2`, for **38** new ids (P0 + 27 + 8 + 2).
  * P0, E1 and E2 also need packs, for **70 packs** in all (≤ 128 under R6).
  * `story_debug_jump_sets_up_every_beat` covers all 38.
* **Unlock macros.**
  * `CUTU()` adds `USTORY_CUT_FIRST + c`, so for c ≥ `CUT_COUNT` (12) it lands on 644 = `USTORY_CH1_DONE`.
  * Add `CUTU2(c) = 677 + (c − CUT_P0)` and `SONGU2(s) = 689 + (s − SONG_MINGLE)`.
  * `test_integration.c`'s existing range check covers both.
* **`SCENE_HOME` args.** 0 and 1 were undefined, and the other homes the player owns had no encoding. The encoding:

  | Arg | Meaning |
  |---|---|
  | `−(1 + HOME_*)` | the player's own home k (−1 = the cottage) |
  | `2 + id` | visit citizen `id` |
  | `0x4000 \| mode` | story modes |
  | 0, 1 | reserved, refused |

**C15. The song table in R16 was wrong.**
* `extern const char doomo_mml_<name>_ch[AUDIO_CH][1]` makes the inner dimension 1, so `ch[1]` points at byte 1.
* A strong definition with a different array type is also undefined behaviour.
* **The fix:** four incomplete arrays per song, `extern const char doomo_mml_<name>_0[], …_1[], …_2[], …_3[];`. These are address constants, which are legal in `songs.c`'s const initializer. The weak placeholders in `stubs.c` are `const char doomo_mml_<name>_N[] = "";`. `AudioSong.ch[]` is unchanged.

**C16. The Feast's day.** `day % 7 == 6` counts days since moving in. Use `doomo_town.weekday ∈ {5, 6}` (Saturday and Sunday; `town.c:16` starts the week on Monday).

**C17. The PS1 vertex cache.**
* RGB565 loses gouraud precision, so a cache entry is 10 B: `sxy` 4, `sz` 2, RGB888 3, pad 1.
* At most 819 vertices fit in the 8 KB tail. The test house needs about 560 (49 floor + about 30 wall + 20 pieces × about 24) ✓.
* The cap is the computed `plat_cache_cap` (C9).

### Determinism and save migration

**C18. Determinism.**
* **The seed** of `story_rng` is `doomo_town.seed` (`town.h:155`), fixed at new game and set by `seed N`. It is never a "save hash", because the save bytes change on every save and the picks would change after a reload.
* **Entropy.** `story.c:186` and `overworld.c:284,299` read `plat_entropy()`. On all three runners (S2 native, H2 GBA, H3 PS1), `seed` also overrides `plat_entropy()` for the next `town_new()`.
* **Context constants.** Visits, the defence waves, HomeMart's stock and the Feast culprit all use `story_rng` with documented ctx constants, and a test checks that the ctx values are unique.
* **Cross-platform parity (I9).**
  * Fingerprints are compared at script checkpoints after `wait-idle`, with inputs keyed to logic ticks (both harnesses already count ticks).
  * `L_PUNCH` ticks must match exactly. GBA logic can fall below 30 ticks/s (19.7 measured on the title menu), but ticks stay deterministic, so the comparison is always in ticks, never in milliseconds.

**C19. Save migration (v1 said nothing about it).**
* **A missing `STRY` in an old save.**
  * `reset()` derives `episode_done` and `home_phase` from `doomo_town.story.quest`, since the 28 old `BEAT_*` ids alias onto episodes.
  * It tells a new town (quest 0 and `!SF_STARTED`) from a migration.
  * This needs `reset()` for missing chunks to run after the town body is decoded. S0 checks that in `save_ext.c` and adds `story_save_migrates_old_beats`: a save at each of the 28 old beats loads at the right episode.
* **The 49 old quests** keep their ids and their 2-bit state in town flags 64..163. Only titles change.
* **Bits assumed zero in old saves.**
  * `story_script.c:1112` sets `SF_NEST0 + (p->extra & 7)`. A map prop with `extra ≥ 4` would have set bits 36-39, which SF §17.1 claims as spare.
  * S2 adds a test that every nest prop on every map has `extra < STORY_NESTS` (4). If that fails, those bits are not reused.
* **`HOME`** has no old version, and **`STRY`** is v1 ✓.
* **The customisation spec's save-version bump** must keep the `[tag][ver][len]` chunk stream; both candidates do.

### Gates, disc and ROM

**C20. The PS1 disc.**
* Task #82 was "the PS1 game fits in 2 MB of **RAM**", not a 2 MB disc. Today the EXE (1.29 MB) plus `OVL.BIN` (870 KB) already exceed 2 MB.
* The "≤ 2 MB game disc" gate is deleted; CD capacity is not a constraint here. The RAM gate is C5.

**C21. The GBA ROM.**
* **The total.** The baseline is 2,500,724 B. The package budgets add up to 26 + 36 + 62 + 64 = **188 KB** of text, not 172 KB. With everything else, the total is about 2.50 + 1.38 + 0.19 (customisation) ≈ **4.07 MB**: above the 4,063,232 B warning and only about 125 KB under 4 MiB.
* **Lever 1 is now part of the plan.** Furniture sprites are LZ-packed per piece and decoded into C7's staging buffer, so path (a) also goes through staging, at about +2-4 c/px. The steady state stays ≤ 15 ms.
* **The decision.** I9 decides at H4 M1 from measurements; LZ is mandatory if the ROM is ≥ 3.9 MB before the customisation lands.

**C22. Table C-G: the fps gates.** These replace every fps number in §7 and §9. The GBA figures are drawn fps from the harness; "G0" means I9's measurement of the same scene at G0.

| Scene | GBA | PS1 (Redux) |
|---|---|---|
| `home_steady` (Live, 4 chibis, 3 animated pieces, HUD only) | ≥ 28 | ≥ 28 |
| `home_rotate` / `home_orbit` | ≥ 20 | ≥ 22 |
| `home_cellar_full` | ≤ 2 consecutive drawn frames below 20; average ≥ 24 | ≥ 24 |
| `home_defend_w3` | ≥ 20 | ≥ 22 |
| `home_modal` (catalog) | ≥ 18 (the shop measures 21-27) | ≥ 26 |
| `home_talk` (dialogue in the house) | ≥ the G0 story dialogue figure − 1 | ≥ the G0 dialogue figure − 1 (21-22 today) |
| `cutscene_wall`, the existing cutscenes, the summit, `feast_pop` | ≥ the G0 figure for the same scene − 1 (dialogue-class scenes run 6.6-8.9 today and the overworld 8.7-12.9, so v1's ≥ 28 / ≥ 24 could not be met) | the summit walk ≥ 27; with dialogue ≥ the G0 dialogue figure − 1; `feast_pop` ≥ 20 |
| new beat-stage cutscenes (P0, F1-F4, the Bottle) | ≥ 15 (Feast: with 60 confetti particles) | ≥ 24 |
| every existing heavy-scene fps script (tasks #18, #35) | no regression greater than 1 fps | the same |

### Content checks and the rest

**C23. Side missions and the cast.**
* **Givers.** An automated pass found 134 "Name (id)" givers in `story_final.md`, and every one matches the name at that id in `roster_data.c` (69 is Prof. Fizz, 729 is Grumbleguts, and so on). The 10 apparent mismatches are counts such as "Stand-alones (25)", not ids.
* **Kevin** has no roster row: his bust is procedural (S3) ✓.
* **Breach/Arena variant unlocks** are rows in `doomo_unlocks_story` at 698..710. They are inside the story range even though they are levels content, and the levels range 864..895 is untouched ✓.

**C24. `story_sprites` on the PS1.**
* The sprites are drawn from the story, cutscene, exorcism and credits code. A function reached from two groups becomes resident along with its rodata.
* S3 keeps pixel tables out: the sprites are procedural, drawn into the scene scratch the way `grumbleguts_gfx` is.
* Each consumer's sprites live in that consumer's own files where possible (exorcism art in the exorcism files).
* The shared remainder is ≤ 4 KB resident (C5). The "40-60 KB of art" budget applies only to GBA ROM.

**C25. Stale or minor items.**
* **Stale:**
  * `core/tests/data/` does not exist yet; S1 creates it for `trailer_curves.h`.
  * `exorcism ≤ 100 KB` and `pet ≤ 100 KB` are lowered to 64 KB (C4).
  * `home ≤ 100 KB` must also satisfy C5's tail rule (≤ 104,448 B).
* **Verified against the tree:**
  * the trailer runs at `TK_FPS 30` (`tk.h:22`), so its frame counts carry over 1:1;
  * `tk.c` uses floats, which is fine because it is host only, so `trailer_curves.c` stays a host tool;
  * `OwScratch` is about 5.6 KB today (40 × 36 B + 256 × 6 B + `OwFxPix`), so 512 B more fits;
  * R7 holds: action's highest used id is 550, story uses 608..676, and foundation 202..239 is spare;
  * R15 holds: `story_feast.c` panels 9+ contain "...that's hot." and the smack / "your turn~" queue lines at lines 997-999, so the cut stands;
  * R21 holds (337 tests, cap 512);
  * `ui_dialog.c` already copies text into 200-character entries (`DLG_TEXT 200`), so only the `done` callbacks escape (C6).

### Summary of package changes made by §12

| Package | Change |
|---|---|
| S0 | adds `SCENE_STORY_MG`, `SCENE_STORY_TIMING`, the `story_mg_table` registry, `BEAT_E1/E2`, the `CUTU2`/`SONGU2` bases, `TIME` and the `HOME` `GRANT`/`PROP_GAG` sub-ops; `save_budget.c` uses the C9 formula; adds `story_save_migrates_old_beats`; leaves `scenes.h` to I9 as maintainer |
| S1 | NPC/camera scripting and the spawner move into `overworld_*.c`; synth SFX instead of PCM; the GBA flip hunk goes in `main.c`; `postfx.c` owns the 2D bbox accumulator; OBJ use is limited by C7.4-6; returns host-kind beat events (C6); adds `ownpc_cast` and the `RIPPLE` act |
| S2 | CMake and `iso.xml` hunks (C3); a 14 KB resident window, the tail window and the stash (C5); `VM_EXT` (C6); `TIME`, `GRANT`, `PROP_GAG`, `STRY.later`; `push_args.txt` (C12); the closed notation list (C14); the seed and entropy override (C18); the migration and nest-bit tests (C19) |
| S3 | `combat_story.c` and `levels_story.c` instead of editing `combat*.c` / `actor.c`; combat growth ≤ +1,936 B; procedural `story_sprites` (C24); the map file and registry hunk |
| S4 / S5 | variants restaged or requested (C12); Grumbleguts through the real resident 729 (C13); the Feast on the weekday and rebuilt from STRY (C9, C16) |
| H1 | `HOME` 3,432 B with cached results and the pending gags; `house_api.c` reads `.bss` only (C6); `.bss` waits for post-G0 ≥ 12,352 B free |
| H2 | the `ow_out` + staging blit paths, LZ sprites if C21 triggers, the opaque-HUD API, the OBJ VRAM claim, 20-rect dirty lists, the C-G gates |
| H3 | `group_of()` extended to generated and platform files, the reachability check, the `overlay.c` tail, 10 B vertex-cache entries |
| H4 | `furngen` emits `furn_grant.c`, `furn_data.c` and `furn_iso.c`; the `SCENE_HOME` arg encoding |
| I9 | the joint PS1 ledger with the customisation work; C-G gates measured against G0; owns `scenes.h` |

*End of SPEC.*
