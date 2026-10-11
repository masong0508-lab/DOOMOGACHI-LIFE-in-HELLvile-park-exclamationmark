# Story Mode as it stands: technical and narrative map (GBA + PS1)

This is a read-only snapshot of the repo working tree at HEAD `43af439`. I compared the story, overworld, cutscene, credits and dialogue sources byte for byte against the 16:22 snapshot build in `scratchpad/snap/repo` and they are identical. All `file:line` refs are relative to the repo root. Sizes marked **(measured)** come from that snapshot's GBA objects (`arm-none-eabi-size`, `-O2 -mthumb`), its GBA `memreport`, or its PS1 `doomogachi.ldmap` / `ovl.ld`. I did not build anything in the repo. The scratch probes I compiled (`itc3.c`, `itc4.c`, `itc5.c`) live in this folder.

---------------------------------------------------------------------------------------------------
## 0. Twelve facts to know before changing anything

1. **The story is not a bytecode engine.** Every beat is hand-written C: `switch` statements in `story_script.c`, plus `static const StoryLine[]` arrays of `{speaker, "text"}`. Lines are chained through C callbacks (`story_say_lines(lines, done_fn)`). The header comment says so: "bytecode-free" (`core/include/doomo/story.h:25`).
2. **The main plot is short.** It has 28 beats (11 / 7 / 3 / 6 plus DONE) and **167 authored lines (~2,140 words)**. The narrator has 51 of those lines and **the player character has 2**. The side quests add about 2,300 words across 49 quests (three strings each), and small talk adds about 1,300 words. `docs/GAME_DESIGN.md:62` targets "~2-4 hours"; the main path is likely about one hour (estimated, not measured).
3. **Four of the 27 plot beats are "talk to N random people" counters:** C1_FRIENDS 3, C2_WEIRD 3, C4_PRISONERS 5 and C4_FESTIVAL 4 (`story_script.c:1195-1210`). C2_WEIRD and C4_FESTIVAL even count repeat chats with the same person.
4. **Choices have almost no consequences.** `SF_VAPE_SAMPLE` and `SF_WEAPON_BRUTAL` are written and never read. `SF_STEAMY_LOST` only hides one side quest. `SF_VEX_REFORMED` and `SF_VAPULA_PLUSH` are declared (`story_int.h:35-36`) and never used. `USTORY_PACIFIST` is never granted. New Game+ is unlocked but not implemented (nothing reads `MODE_NEW_GAME_PLUS`).
5. **Cutscenes are captioned slideshows.** There are 4 chapter cards plus 7 vignettes, built from 20 caption panels. They have no camera, no actor system, no per-character timing, no portraits and no easing curves beyond `ui_wave` and `ui_pop_scale`.
6. **Dialogue delivery is flat.** The box uses one portrait expression (TALK flapping with NEUTRAL; POSSESSED is automatic). There are no per-line emotes, pauses, shakes, SFX stings or text effects. This is the main reason the jokes don't land, compared with the trailer.
7. **Overworld NPCs can only stand, turn, idle-bob and wander within ±2 cells of home.** There is no scripted "walk to X", no emote-on-command, no camera pan and no player auto-walk. The player only reaches screen shake, flash, banner and fade through `ow_shake`, `ow_flash`, `ow_banner` and `fade`.
8. **No house interior, furniture or decorating system exists anywhere** (story or free play). `PLACE_HOME` is a menu with "Rest & save", "Activities" (Peek at a Dream, night only) and "Leave". The only interior art is the 2-panel `CUT_HOME` vignette.
9. **GBA: ROM is roomy, EWRAM is not.** The ROM is 2.37 MB (padded to 4 MB). EWRAM has **7,680 B free (measured)** and is shared with the 12 KB scene scratch. Story text costs ROM only. Persistent state costs EWRAM byte for byte.
10. **PS1: most of the story engine is resident, not overlaid.** The "story" overlay group is **49,776 B (48.6 KB, measured)** and holds only the overworld, cutscene, credits and questlog scene code. `story_script.c`, `story_side.c`, `story_talk.c` and their tables and text (about 73 KB) stay resident, because their callbacks escape into the dialogue queue. Resident growth eats the ~75 KB margin above the 64 KB stack reserve. Overlay-group growth is free up to the slot size, **111,376 B (108.8 KB, set by the combat group)**.
11. **Save space is nearly full.** The 256 story flag bits are used up to bit 227, leaving bits 228-255 (28 bits) plus `SF_31_FREE`. Side quests are capped at 50 by the flag layout (49 used). `ITEM_COUNT` is 63 against a cap of 64 (one item id free). `PROG_COUNT` is 32 against a cap of 32 (no progress counters free). The story has no `save_ext` chunk yet. New persistent story or home state needs one (`DOOMO_SAVE_EXT_MAX` 8 KB per chunk; the GBA story flash slot is at most 40 KB).
12. **Several authored threads are set up and never paid off:** each demon's "true name" belongs to a real Hellvile citizen, Vex's redemption, the deflated Vapula, Hattie the possessed comedian, and Rivet's "Back in Hellvile I welded gates". These are the cheapest places to add heart and laughs.

---------------------------------------------------------------------------------------------------
## 1. Files, sizes and where the bytes live

### 1.1 Story module source files

| File | Lines | Role |
|---|---:|---|
| `core/include/doomo/story.h` | 210 | public API: args, chapters, beats, cutscene ids, side-quest API, places, map tags, flag map |
| `core/src/story_int.h` | 220 | private: `SF_*` flags, cast ids, pending sub-scenes, `StoryLine`, NPC spots, unlock ids |
| `core/src/story.c` | 607 | state machine, flags, beats, conversations, sub-scene dispatch, rewards, friendship, debug hooks |
| `core/src/story_script.c` | 1561 | **the whole main plot** (text plus logic per beat) |
| `core/src/story_side.c` | 709 | side-quest table (49 rows) plus generic quest logic |
| `core/src/story_talk.c` | 321 | ambient small talk generator plus cast idle lines |
| `core/src/story_places.c` | 403 | place names, menus, mini shops, signs, region names, flower beds |
| `core/src/story_questlog.c` | 238 | `SCENE_QUESTLOG` (Story / Side / Items / Map / Friends) |
| `core/src/cutscene.c` | 531 | `SCENE_CUTSCENE`: chapter cards plus 7 vignettes |
| `core/src/credits.c` | 282 | `SCENE_CREDITS`: parade of 1000 citizens |
| `core/src/overworld.c` | 830 | `SCENE_STORY`: player, camera, doors, triggers, pause, draw |
| `core/src/overworld_npc.c` | 407 | cast, ambient citizens, demon packs, props, secrets |
| `core/src/overworld_fx.c` | 644 | markers, emote bubbles, particles, fireworks, HUD, minimap, pause panel |
| `core/src/overworld_int.h` | 143 | overworld private state |
| `core/src/ui_dialog.c` | 264 | `SCENE_DIALOG` (owned by foundation, used by everything) |
| `core/src/unlocks_story.c` | — | unlock table 608..735 |
| `core/src/audio.c`, `songs.c`, `core/include/doomo/audio.h` | — | sequencer, 19 songs, 35 SFX, voice babble |
| `core/data/maps/{hahaland,hahaland_invaded,hellvile,park_united}.txt` | — | 64x64 overworld maps (compiled to `core/gen/map_*.c`) |

### 1.2 GBA (measured; THUMB `-O2`; everything const is in ROM)

| Object | code | tables | **strings** | .bss (EWRAM) |
|---|---:|---:|---:|---:|
| story.o | 4,188 | 501 | 507 | 23 |
| story_script.o | 11,892 | 5,590 | **12,265** | 72 |
| story_side.o | 3,436 | 3,143 | **12,233** | 0 |
| story_talk.o | 1,932 | 890 | **6,930** | 52 |
| story_places.o | 2,376 | 2,038 | 2,228 | 436 |
| story_questlog.o | 2,848 | 500 | 516 | 187 |
| cutscene.o | 7,484 | 551 | 1,374 | 16 |
| credits.o | 2,744 | 327 | 979 | 12 |
| overworld.o (core) | 7,640 | 783 | 81 | 216 |
| overworld_npc.o | 5,104 | 62 | 0 | 52 |
| overworld_fx.o | 7,100 | 353 | 0 | 589 |
| **story module total** | **~56.7 KB** | **~14.4 KB** | **~37 KB** | **~1.65 KB** |
| ui_dialog.o | 2,360 | 81 | 7 | **3,065** (8-entry queue) |
| unlocks_story.o / audio.o / songs.o | 0 / 5,980 / 0 | 1,392 / 948 / 772 | 1,981 / 309 / 10,738 | 0 / 374 / 0 |
| 4 story maps (`core/gen/map_*.o`) | — | 127 KB of cells | — | — |

* **GBA memreport (snapshot 16:22):** ROM 2,428,644 B; IWRAM has 6,460 B left for the stack; **EWRAM 254,464 / 262,144 used, 7,680 B free.** Biggest items: `doomo_town` 109,524 (includes `DoomoStoryState`, 62 B); `doomo_map` 32,780; portrait `pool` 18,432; `gba_big` 17,408; **scene scratch 12,288**; ... `dq` (dialogue) 3,040. Other agents are changing GBA memory right now, so re-measure before committing to numbers.
* **Overworld scratch:** `OwScratch` = **5,644 B** (40 NPCs x 36 B, 256 props x 6 B, 2,665 B of generated marker, emote and minimap pixels). It lives in the shared 12 KB `scene_scratch` (`overworld_int.h:96-110`, `overworld.c:20-57`), so about 6.6 KB of the scratch is free while the overworld owns it. Any pushed scene may take the scratch, and the overworld rebuilds afterwards (`ow_scratch_ensure`, `overworld.c:51-57`). `OwState ow` is 216 B of .bss.
* **Flash:** story slot 0 holds at most 40 KB (`docs/GBA.md`, flash layout).

### 1.3 PS1 (measured from the snapshot `doomogachi.ldmap` / `ovl.ld`)

* **Overlay slot:** `__ovl_slot` at `0x801c1b30`, sized by the largest group, combat `.ovl_01` = 0x1b310 = **111,376 B (108.8 KB)**. Free RAM (`docs/PS1.md:237`): the image ends `0x801dd310`, **139 KB below the stack top**, which is **about 75 KB more than the 64 KB stack reserve**. `OVL.BIN` holds 15 groups.
* **Story group `.ovl_13`** = 0xc270 = **49,776 B (48.6 KB)**, about 60 KB under the slot size. It contains (`ovl.ld:313-324`):
  * credits, cutscene, `overworld_fx` and most of `overworld.c` / `overworld_npc.c` (the scene functions and their draw helpers);
  * from story*.c only the code reachable *only* through scene entry points: `script_debug_setup`, `script_nest_encounter`, `script_target`, the `side_spots` / `story_side_*` getters, `place_sign_text`, `story_region_name`, `story_pick_flowers`, and the questlog scene.
* **Resident story bytes (everything else):**

| File | resident text | resident rodata | overlay text + rodata |
|---|---:|---:|---:|
| story.c | 5,212 | 904 | 856 |
| story_script.c | **12,364** | **17,788** | 1,356 |
| story_side.c | 4,308 | **15,164** | 1,376 |
| story_talk.c | 3,012 | 7,832 | 0 |
| story_places.c | 2,804 | 3,400 | 1,536 |
| story_questlog.c | 0 | 536 | 4,612 |
| cutscene.c / credits.c | 48 / 0 | 1,448 / 1,036 | 11,648 / 5,636 |
| **story*.c resident** | **~27.7 KB** | **~45.6 KB** | — |

  On top of that, `ui_dialog.c` (3.3 KB code plus 3 KB .bss), `audio.c` and `songs.c` (~20.5 KB) and `unlocks_story.c` (3.4 KB) are resident.
* **Why it is resident.** `ovl_plan.py` keeps any section whose address escapes into memory: callbacks handed to `ui_say_done` / `ui_choice` (`ui_dialog.c:72-89`), and the `StoryLine` tables those callbacks reach. Nearly every `script_*` / `side_*` function is either such a callback or called by resident code (`story_interact_npc` is called from `combat.c:1123`, `fp_*`, and others). **A data-driven script with no function pointers into scene code could be overlaid. The current callback style cannot.**
* **PS1 scene switches cost CD reads.** About 0.3 s per 70 KB, and a switch from one overlaid scene to another costs three reads (`docs/PS1.md:169-198`). Story to combat to story loads the 109 KB combat group plus the 49 KB story group. Story to cutscene, and story to dialogue, cost nothing (same group, or resident). **Comedic smash cuts must stay inside the story group, or the PS1 stalls mid-gag.**
* **The overworld renders differently per platform.** On PS1 it is real 3D terrain from a tilted third-person camera: about -58° pitch, telephoto, billboards at 2x (`ps1/src/overworld.c:1-30`). On GBA it is top-down 16 px tiles with lifted block front faces, a cheap 3/4 look (`gba/src/overworld.c:1-20`). Both read the same 64x64 cell maps.

---------------------------------------------------------------------------------------------------
## 2. The state machine (`story.c`, `story.h`, `story_int.h`)

* **Saved state:** `DoomoStoryState` (`core/include/doomo/town.h:72-84`, 62 B) holds `chapter`, `map`, `x`/`y`, `facing`, `equipped`, `hp`, `armor`, `ammo[8]`, `quest` (= `BEAT_*`), `quest_step` (a small counter), `kills`, and `flags[32]` (256 bits).
* **Chapters:** `STORY_CH_NONE`, `CH1`..`CH4`, `CH_DONE` (`story.h:69-70`). Titles are "Welcome to HaHaland", "Something in the Vapor", "Hell Invades", "Hellvile Park" and "Happily Ever After" (`story.c:32-35`).
* **Beats:** 28 in all (`story.h:75-91`), mapped to chapters by `beat_ch[]` (`story.c:37-43`). `story_set_beat()` (`story.c:108-122`) runs `chapter_unlocks()` for each chapter passed (songs go to the jukebox and `USTORY_CHn_DONE` is granted, `story.c:86-106`), resets `quest_step` and calls `script_enter_beat()`.
* **Flag map** (`story.h:193-208`, `story_int.h:13-47`):

| Bits | Use |
|---|---|
| 0..31 | plot one-shots `SF_*`: `SF_STARTED` … `SF_INVASION_HINT`, plus `SF_31_FREE` |
| 32..35 | ch3 nests cleared (bits 36-39 unused) |
| 40..41 | ch3 exorcisms (42-47 unused) |
| 48..63 | "met" / tutorial (`SF_MET0+8` = Tallyworn met, `+9` = Mother Nox met; bits 48-55 and 58-63 unused) |
| 64..163 | side quests, 2 bits each (50 max) |
| 164..195 | secret sparkles (HaHaland 0-7, Hellvile 16-23, park 24-31) |
| 196..227 | places visited (26 places) |
| 228..255 | reserved |

* **Event dispatch** (`story.c:512-560`):
  * `story_interact_npc(id)` tries `script_npc` (main plot), then `side_npc`, then `talk_ambient`.
  * `story_trigger(tag)` handles door tags 64+`PLACE_*` (via `story_enter_place`), secret tags 135-142 (reward plus toast), and otherwise calls `script_trigger`.
  * `story_enter_place(p)` marks the place visited (explorer unlock), then tries `script_place`, then `side_place`, then `story_place_menu`.
* **Pending sub-scenes** (`story.c:353-504`): `story_push_scene`, `story_push_combat`, `story_push_exorcism` and `story_push_cutscene` record `story_pending {kind, ctx, arg, extra}`, sync the overworld to the save, and push the scene. When `SCENE_STORY` is back on top (`overworld.c:639-643`), `story_resume()` computes the result (`pending_result`, `story.c:361-378`: combat `combat_last_result()`, exorcism by resident status 1/0/-1, pet exists, creator done) and calls `side_resume` (`CTX_SIDE`) or `script_resume`.
* **Story combat setup** (`story.c:455-482`): `combat_start_at(map, CGOAL_KILL_ALL, cx, cy, 3072)` runs on the **same overworld map**. It drops props beyond the nearest 72 (`make_room_for_demons`, `story.c:429-453`) and spawns a demon set from `nest_demons[6][4]` (`story.c:409-416`): 4 nests plus 2 hunt sets.
* **Autowin and debug** (`story.c:579-605`): `story_debug_autowin(1)` resolves combat, exorcism, pet and workshop instantly. `story_debug_jump(ch, beat)` calls `script_debug_setup` (`story_script.c:1443-1491`). `story_debug_step()` calls `script_debug_step` (`story_script.c:1506-1561`), so tests can play the whole arc.
* **Friendship** (`story.c:193-225`): a citizen is a friend at `level >= STORY_FRIEND_LEVEL` (2). The first ambient chat gives +2, which makes a friend instantly. 25 friends grants `USTORY_FRIEND_OF_ALL`.
* **Rewards:** `story_reward(money, item, n, unlock)` shows a toast and plays `SFX_PICKUP` (`story.c:228-247`).

---------------------------------------------------------------------------------------------------
## 3. The "script engine" (`story_script.c`)

**Storage.**
* `typedef struct StoryLine { int16_t who; const char *text; }` (`story_int.h:102`).
* `who` is a resident id, `-1` (narrator, no portrait) or `DOOMO_PLAYER_ID`.
* There are 54 `L_*` arrays terminated by `SAY_END` (`story_script.c:62-460`), plus option arrays `O_*` (`O_vape` 171, `O_exorcise` 213, `O_weapon` 238, `O_descend` 258, `O_fog` 818, `O_travel_*` 888-890).
* Text is const in ROM (GBA) or resident RAM (PS1). Main-plot strings total 12,265 B on GBA.

**Opcodes.** There are none. The equivalent operations are C calls:

| Operation | Call |
|---|---|
| say lines then call a function | `story_say_lines(L, done)` (`story.c:334-340`); queues at most 6 lines per batch (`conv_feed`, `story.c:315-332`) because the dialogue queue holds 8 (`ui_dialog.c:15`) |
| one line | `story_say(who, fmt)`; 200-char buffer (`story.c:302-307`) |
| choice | `story_choice(who, q, opts, n, cb(int))` (`story.c:342-348`; at most 6 options of 27 chars, `ui_dialog.c:17-18`) |
| set beat / flag / step | `beat(b)`, `flag(f)`, `story_set_step(n)` (`story_script.c:15-18`) |
| possess / unpossess | `possess(id)` / `unpossess(id)` (`story_script.c:32-49`); writes resident status plus corruption |
| cutscene | `story_push_cutscene(CUT_*, ctx, extra)` |
| combat | `story_push_combat(MAPID_*, ctx, extra, cx, cy, nest)` |
| exorcism | `story_push_exorcism(id or -1, ctx, extra)` |
| other scene | `story_push_scene(&SCENE_X, arg, PEND_*, ctx, extra)` (pet, workshop, credits, questlog, ...) |
| give item / reward / friendship | `inv_add`, `story_reward`, `story_befriend` |
| teleport | `ow_enter_map(map, cx, cy)` |
| refresh NPC placement | `ow_refresh_cast()` |

**Text tokens** (`story_fmt`, `story.c:261-295`):

| Token | Expands to |
|---|---|
| `@P` | player name (default "Sunny", the bunnykin from `default_player`, `story.c:137-154`) |
| `@E` | pet name or "your egg" |
| `@T` | target's name (`story_fmt_target`) |
| `@D` | the possessing demon's name (= a Hellvile citizen, see §11.3) |
| `@V` | flavour of the day from a 6-name list (`story.c:250-253`) |

Small talk has its own `{N}{J}{F}{H}{M}{X}{C}{B}{Y}{P}{E}` tokens (`story_talk.c:15-53`).

**Per-beat hooks.** Each hook is a `switch` on the current beat:

| Hook | Lines | Purpose |
|---|---|---|
| `script_npc` | 681-813 | talking |
| `script_trigger` | 825-883 | walk-on cells |
| `script_place` | 916-1002 | doors |
| `script_resume` | 1009-1140 | sub-scene results |
| `script_enter_beat` | 1145-1191 | auto actions on entering a beat |
| `script_ambient_talked` | 1195-1210 | the counters |
| `script_objective` | 1215-1274 | HUD text |
| `target_of` / `script_target` | 1279-1342 | compass / marker target |
| `script_cast` | 1358-1431 | who stands where, per map and beat; flags `NPCF_FIXED`/`MAIN`/`SIDE`/`READY`/`DEMON`, `story_int.h:133-137` |
| `script_debug_setup` / `script_debug_step` | 1443-1561 | debug setup and stepping |

**Hard-coded positions.** Cells for targets and cast are literals scattered through `target_of` and `script_cast`, with matching debug placements.

---------------------------------------------------------------------------------------------------
## 4. Cutscenes (`cutscene.c`): what they can and cannot animate

**Data.** `Panel {who, min_t, text}` (`cutscene.c:10`). The scripts are at `cutscene.c:13-54`, the table at 56-58, and the gallery names at 60-64.

| CUT | Panels | Music | Picture |
|---|---|---|---|
| CHAPTER1..4 | 1 (no text) | TITLE / VAPE_LOUNGE / INVASION / HELLVILE | `draw_card` (206-250). "Chapter N" pops in with `ui_pop_scale`; the title appears after 18 ticks. Ch2 has drifting vape puffs; ch3 has rising embers, a red flash every 60 ticks and a title jitter; ch4 has a half-purple checker and a beating heart. Sparkles. "A Continue" after 60 ticks |
| ARRIVAL | 3 | HAHALAND_DAY | `draw_arrival` (269-297): pastel sky, a pink bus driving in (`ct*2`), and the player figure hopping in panel ≥ 1 |
| HOME | 2 | PET | `draw_home` (299-328): tiled `TEX_WALLPAPER_HEARTS` plus `TEX_WOOD_FLOOR`, a window, a bed, and an egg wobbling at 3x with an orbit of sparkles. `SFX_EGG_CRACK` every 30 ticks |
| FOG | 3 | OMINOUS | `draw_fog` (336-361): rust wall, 7 drifting vape puffs, two blinking red eyes, "hee hee hee" text wobble. `SFX_POSSESS` |
| ERUPTION | 4 | OMINOUS, then INVASION | `draw_eruption` (363-404): sky bands shift pink → purple → blood. The cloud grows and grins. **Screen shake** = `((ct*7)%7)-3` from panel 2, plus lightning lines, a white flash, and 5 demon silhouettes bobbing; "!!!". `SFX_EXPLODE` and `SFX_DEMON_ROAR` |
| VAPULA_DOWN | 3 | VICTORY | `draw_vapula` (406-438): Vapula sprite (pain, then death frames), puffs blooming outward, a small walk frame, confetti |
| WALL_FALLS | 3 | FESTIVAL | `draw_wall_falls` (440-490): **the best animation in the game**. A brick wall shudders, then each brick falls under quadratic gravity with staggered delays and dust puffs. Then 4 cast members run in from each side (`bodyspr_image` walk frames) and hearts rise. `SFX_EXPLODE` every 16 ticks |
| FREEPLAY | 2 | CREDITS | "THE END ...of the beginning!" with fireworks |

**Timing model** (`cs_update`, `cutscene.c:108-129`):
* `ct` counts ticks in the cutscene and `pt` ticks in the panel.
* A panel **auto-advances at `min_t + 200` ticks** (about 8-9 s). A or B is accepted after 12 ticks and jumps straight to the next panel. START skips the whole cutscene.
* Captions typewrite at a fixed **2 chars/tick** in a `ui_window_pop` box (`caption`, 186-203), styled cream (dark for fog and eruption).
* Hard-coded per-cut SFX cue points (`113-118`) are the only "timeline".

**What is missing** (compare the trailer, §15):
* no camera (pan, zoom, truck), no per-actor tracks or keyframes, no easing library (only `ui_wave` sine and the fixed `ui_pop_scale` overshoot)
* no portraits or expressions (`ui_portrait` is never called; only `figure_image`, `bodyspr_image` and `sprite_image`)
* no speaker per panel (`who` is always -1), no freeze frames, holds or beats, no impact frames, no lip sync
* no shake helper (shake is ad hoc in one vignette), no palette rot (`TINT_*` exists only in the overworld view)
* captions cover the bottom of the art

`cs_end` quietly grants gallery unlock `USTORY_CUT_FIRST + cut` for the vignettes (`79-84`).

---------------------------------------------------------------------------------------------------
## 5. The dialogue UI (`ui_dialog.c`, shared with every module)

* `ui_say(id, text)`, `ui_choice(id, q, opts, n, cb)` and `ui_say_done(cb)` (`ui_dialog.c:67-89`) use a queue of **8 entries x ~380 B** (200-char text plus 6x28 options): 3,040 B of EWRAM. Callbacks run after the box closes, so they may push scenes (`advance`, 142-152).
* **Typewriter** (165-169): `OPT_TEXT_SPEED` gives 0 = 15 chars/s, 1 (default) = 30, 2 = 60, 3 = instant. A or B completes the page, then advances. Long text pages automatically by box lines: 3 lines at 240x160, 4 at 320x240 (`dlg_box`, 103-113).
* **Voice babble** (170-178): `audio_voice_char()` per typed char, using the speaker's 5 voice sliders (a fixed narrator voice for -1).
* **Look** (202-262): the box slides up with a small overshoot. The speaker portrait sits above the box. **Expression is only `EXPR_TALK` alternating with `EXPR_NEUTRAL` every 4 ticks while typing, or `EXPR_POSSESSED` if possessed** (220-227). There is a name pill, a bobbing heart "next" marker, and a choice window that pops out at the top right.
* Portraits *support* 11 expressions: NEUTRAL, HAPPY, SAD, ANGRY, LOVE, SHOCK, SLEEPY, SICK, POSSESSED, DEAD, TALK (`portrait.h:31-33`), with breathing, ear twitch and tail wag. **The dialogue never asks for anything but TALK, NEUTRAL or POSSESSED.**
* There is no inline markup: no `{pause}`, `{shake}`, `{big}`, `{sfx}` or `{face=...}`. The box style is per map (`restore_style`, `overworld.c:152-156`: CUTE, DARK in Hellvile, HELL when invaded).

---------------------------------------------------------------------------------------------------
## 6. The overworld (`SCENE_STORY`): what can move and how

**Player** (`overworld.c:401-451`):
* walks (`WALK_SPEED` 4.5 units per tick) or runs (B, 8 units per tick) in 4 directions with box collision against walls, water, lava, steps over 3 height units, props and NPCs;
* entering a door takes 6 ticks of pushing (`427-436`);
* stepping on a new cell fires region banners, `CELL_TRIGGER` tags and side-quest spots (`on_new_cell`, 388-399);
* animation: 4-frame `bodyspr_image` walk, plus a 2 px breathing bob when idle (764-770).

**Pet:** follows the player's position trail 6 samples behind with 1/5 lerp, hopping while moving (565-571, 749-761). The egg sprite is shown before hatching.

**Camera:** lerps 1/4 toward the player, clamped to the map (554-563). **There is no scripted camera.**

**Screen effects:**

| Effect | Where | Notes |
|---|---|---|
| `ow_shake(ticks)` | `overworld.c:68`, 679-683 | ±3x2 units jitter, decays per drawn frame |
| `ow_flash(color, ticks)` | `overworld.c:69` | checker overlay |
| `ow_banner(text)` | `overworld.c:70` | region or map title |
| `ow.fade` | — | used on map enter and after sub-scenes |
| tint | `ow_view_fill`, 773-802 | red, pulsing on the invaded map; purple in Hellvile; purple creeping near the Wall in ch1-2, by x-cell |

**NPCs** (`overworld_npc.c`):
* There are 40 slots. They come from `script_cast`, then `side_cast`, then **demon packs on nests**, then **ambient citizens** on the map's `THING_NPC` wander spots (`ownpc_spawn_all`, 177-218). Hillvile citizens appear in HaHaland, Hellvile citizens in Hellvile, and both in the park. Only 1 in 3 appears when invaded ("folks are hiding"), and half appear before C1_KEY.
* **States** (`ownpc_update`, 260-341):
  * `NS_IDLE`: fixed NPCs turn to face the player within 100 units or turn randomly.
  * Wanderers pick a random target within ±2 cells every 40-160 ticks.
  * `NS_WALK` moves at a speed from the persona `MOVEMENT` slider (`new_npc`, 138-141).
  * `NS_TALK` faces the player for 60 ticks after a chat.
* **Demons** sway on sine waves around their nest (265-269).
* **Animation:** 4-dir x 4-frame walk, breathing bob, ghosts translucent (378-385).
* **Bubbles:** quest markers (`!` main, `?` side, heart ready) bob above heads (386-388). Emote bubbles appear within 200 units: TALK, HEART, NOTE, ZZZ, SWEAT, SKULL (`EMO_*`, `overworld_int.h:42`). They are chosen automatically from status, mood, night and map (329-339).
* **There is no API to make an NPC walk to a point, jump, spin, emote on cue, follow the player, or play a custom frame.**

**FX** (`overworld_fx.c:1-14`): generated glossy bubble images; screen-space weather or mood particles (petals by day, fireflies at night, purple wisps near the Wall, embers and ash when invaded, bats over Hellvile, confetti plus fireworks at the festival); HUD (objective pill, clock and money, region banner, A prompt, compass arrow to `story_target`, minimap); the pause panel.

**Auto-events** (`auto_events`, 332-366): the chapter 1 card and arrival, the forced move to the invaded map in C3, and one-shot toasts (controls, pet follows, walk into a nest).

**Pause menu** (574-622): Resume, Quest Log, Doomogachi (`SCENE_PET`), Armory (`SCENE_WORKSHOP`), Map (questlog tab 3), Options, Save, Title Screen.

**Props** (A on them, 498-522): signs (`place_sign_text`, `story_places.c:321-364`: 23 signs, including joke graves), flower beds (one bunch per bed per day), mailbox, giggling fountain.

**Music** (`ow_music_for_map`, 138-150): invaded = INVASION, Hellvile = HELLVILE, park = FESTIVAL. HaHaland plays day or night, but OMINOUS during C1_WALL and C2_SCREAM..C2_EXORCISE.

---------------------------------------------------------------------------------------------------
## 7. Places, small talk, quest log

**Places** (`story_places.c`):
* 25 places (`story.h:154-163`, names `story_places.c:13-21`).
* The menu (`open_menu`, 259-286; at most 6 options) offers: Talk to keeper (`keeper_of`, 49-72), Buy (mini shops, 75-95, 4 items per page), Weld & armory (workshop and forge from C2_CELLAR), Rest & save (home, apartments, pits: sleep until 07:00, full heal, `save_game(0)`, 202-216), Get patched up (hospital, 218-226), Browse every shelf (`SCENE_SHOP`), Visit the graves (`SCENE_GRAVEYARD`), Activities (`activities_at(loc)`, ACTF_STORY plus min chapter, 137-148; "Coming soon!" if none), Leave.
* **Activities reachable in story:**
  * Fun Fair: minigames (`mg.c:609+`)
  * Concert Hall: Concert, Hit Parade
  * Home and apartments: Peek at a Dream (night only)
  * Pier and Bone Pier: Fishing, Fishdex
  * Cafe and Stalls: Cook with Granny, Serve Customers
  * Library
  * Arena: Hell Breach Levels, Boss Rush, Horde, Time Trial
  * Hellvile: Underbelly Gate (Breach 7), Graveyard Shift (Breach 5, night)
  * Town Hall: Music Studio
* Region names come from 24 rectangles (`story_places.c:368-393`).

**Small talk** (`story_talk.c`):
* `talk_ambient` (285-321) says a greeting picked by persona speech and quirk, then a topic, then on first meeting a narrator bio line "(Name the Job: bio)". The first chat makes a friend.
* Topics (72-152): life (job, food, hobby, music, fear, catchphrase), chapter 2 weirdness, invaded, Hellvile (vapor years), softie, loyalist, festival, night, possessed ("Leave a message after the scream."), ghost.
* Lines are selected by a hash of id, variant and day (156-193).
* The cast says **48 per-chapter idle lines** instead (`cast_lines`, 196-246; one is picked per 7 minutes of game time, 248-264).

**Quest log** (`story_questlog.c`):
* Story tab: chapter, objective, a progress bar over all beats, and checkmarks against the "nice" beat names (78-115).
* Side tab: active, offered, done, with the giver's portrait and hint (117-152).
* Items, Map (`owfx_minimap`), Friends: a 12-cast grid with silhouettes until met, plus friend counts per district (194-215).

---------------------------------------------------------------------------------------------------
## 8. Side quests (`story_side.c`)

**System.**
* One table row per quest (`SideQuest`, 12-30): giver, chapters `ch_min..ch_max` (5 = post-credits), giver map and cell, kind, target, other, count, target spot, reward (H$, item, unlock), and three strings (**offer / hint / thanks**).
* State is 2 bits at flag 64+2q (302-313).
* `available()` (327-351) gates quests by chapter, giver alive and not possessed, target alive, and "plot first" (ch1 only from C1_FRIENDS; ch2 exorcisms from C2_RIVET; ch3 from C3_NESTS; Hellvile givers from C4_PRISONERS).
* **Kinds and their generic handling** (`side_npc`, 481-577):

| Kind | Handling |
|---|---|
| FETCH | the giver checks the inventory count |
| DELIVER | the item is added on accept; the target says one shared generic line, "For me? Oh! Thank you so much! Please tell them I said so!" (493) |
| MATCH | one generic line plus `sim_confess(…, 45)` (502-505) |
| RECONCILE | one generic line, plus +45 relationship and friend status (511-520) |
| EXORCISE | **accepting the quest possesses the target** (`on_accept`, 422-430); the exorcism runs and the result is reported (527-528, 659-663) |
| FIND / PET / PHOTO | a sparkle spot on the map; step on it (or press A for photos, which also flashes) (604-632) |
| VISIT | entering the place marks it (635-649) |
| HUNT | combat on the invaded map with a demon set, or the Spire (462-471) |

* Turn-in: thanks line, +4 friendship, reward, toast, `SFX_HAPPY` (`finish`, 436-448). Completing all 49 grants `USTORY_ALL_SIDE`.
* Counts: **49 quests** (the file header says 46, `story.h` says "more than 40", `docs/STORY.md` says 49). By first chapter: 12 / 9 / 10 / 13 / 5. Every kind is used. By the giver's district: 34 Hillvile and 15 Hellvile. By where the giver stands: 32 on the HaHaland map, 12 in Hellvile, 5 in the park.

**Every quest** (q#, title, giver (id), chapters, kind, one-line summary):

| # | Title | Giver | Ch | Kind | What |
|---|---|---|---|---|---|
| 0 | Mail Mix-Up | Lilypad Lou (97), frog mail carrier | 1-2 | Deliver | love letter stuck to his hat → Valentina |
| 1 | Coffee for Bluebell | Bluebell (29), florist | 1-2 | Fetch | 1 Coffee for the tulip-heart arranger |
| 2 | Lost Kitten | Barkley (96), dog walker | 1-2 | Pet | find Sir Whiskers up on Hillvile Lane |
| 3 | Say Cheese! | Shutterbug (46), photographer | 1-5 | Photo | the giggling fountain for a postcard |
| 4 | Cupid's Little Helper | Smoochy (117), cupid courier | 1-2 | Match | pep-talk Freckles (11) into confessing to Skipper (25) |
| 5 | Strong Bones | Bonejangles (5), skeleton xylophone teacher | 1-2 | Fetch | 2 Milk "to keep these bones ringing true" |
| 6 | Bedtime Stories | Story Owl (16) | 1-5 | Visit | Library, for 'The Very Hungry Demon' |
| 7 | Ribbon Cutting | Ribbonetta (83) | 1-2 | Visit | be the Fun Fair's official visitor |
| 8 | Grandma's Glasses | Oma Fluff (27) | 1-2 | Find | glasses at the beach ("or was it 1974?") |
| 9 | Sandcastle Contest | Tidepool (112), surfer | 1-5 | Photo | the view from the end of the pier (title mismatch) |
| 10 | Clean Sweep | Squishy (39), slime window washer | 1-2 | Fetch | 3 Soap |
| 11 | Choir Practice | Hallelujah (142) | 1-5 | Visit | test the Concert Hall echo → hymn for the jukebox |
| 12 | Smoke Signals | Drizzle (57), weather bunny | 2 | Photo | the smiling purple cloud over Cloud Nine |
| 13 | Steam Cleaning | Lather (41), soap maker | 2-4 | Deliver | lavender soap → Steamy after his ordeal (hidden if Steamy was lost) |
| 14 | Holy Supplies | Sr. Lumen (354), exorcist | 2-3 | Fetch | 2 Holy Water → a rosary that made a demon sneeze for an hour |
| 15 | Scrap Science | Prof. Fizz (69) | 2-4 | Fetch | 3 Scrap for a vapor detector that beeps at everything |
| 16 | Nervous Nellie | Pillowpuff (23), hug therapist | 2 | Exorcise | Lagspike the streamer (text says "his", roster bio says "her") |
| 17 | No Vapes at Daycare | Miss Lulla (36) | 2-3 | Find | the vape pen the kids think is a wand |
| 18 | A Taste of Home | Rivet (13) | 2-3 | Fetch | 2 Candy Eyeballs: "...Just like Mum's." (sets up Anvil) |
| 19 | Knightly Hearts | Valentina (52), matchmaker | 2-5 | Match | Dame Fluff (9) → Valkyrie (86) |
| 20 | Pip's Bravery Badge | Pip (42) | 2 | Photo | the scary Wall for Mr. Flops |
| 21 | Fire Drill | Chief Spark (99) | 3 | Hunt | brute on the fire hydrant (nest set 4) |
| 22 | Barricade the Bakery | Nana Pudding (1) | 3 | Fetch | 3 Scrap to protect the sourdough starter |
| 23 | Medic! | Dr. Bloom (111) | 3 | Fetch | 2 Medicine |
| 24 | Hymn of Hope | Cantor Coo (426) | 3-4 | Exorcise | Bellwether, whose bells toll BACKWARDS |
| 25 | Mr. Biscuit | Wobbles (127), toddler | 3-4 | Pet | the tiny puppy near the graveyard |
| 26 | Graveyard Shift | Sr. Lumen (354) | 3 | Hunt | cloud spiders **at the Fun Fair** (title mismatch; also a Breach 5 name) |
| 27 | Spire Scout | Updraft (17), balloonist | 3 | Photo | the foot of Cloud Nine Spire |
| 28 | Stay Awake, Guards! | Valkyrie (86) | 3 | Fetch | 2 Energy Drinks |
| 29 | Comfort Food | Sandwichy (4) | 3-4 | Deliver | onigiri → Hootenanny ("hoo" = thank you) |
| 30 | Save the Mascot | Twisty (43) | 3-4 | Exorcise | Hugsley, who "hugs TOO HARD... before he hugs a bus" |
| 31 | Postcard from the Fountain | Clinker (753), chalk poet | 4-5 | Photo | HaHaland's fountain ("forty poems") |
| 32 | Charcoal Cookies | Charlotte (566) | 4-5 | Deliver | burnt bagels → Nana (pays off in Nana's ch5 idle line, "Burnt and Sweet") |
| 33 | The Florist's Apprentice | Morguelet (582), ghoul kid | 4-5 | Reconcile | ask Bluebell to take her on (Pip's ch5 idle line names her) |
| 34 | Game Night | Graveley (730), ghoul landlord | 4-5 | Fetch | Board Game for the crypt kids |
| 35 | Lullaby for Clouds | Ploddy (701), cloud hauler | 4-5 | Visit | the Concert Hall on his behalf |
| 36 | Squeaky Toy Diplomacy | Big Snarl (855), hellhound bouncer | 4-5 | Fetch | a Plushie ("Can I keep it after?") |
| 37 | Last Puff | Dirgelet (766) | 4-5 | Exorcise | Hazey (788), the smoke herald |
| 38 | Counting Together | Tallyworn (712) | 4-5 | Reconcile | meet Tock (104) the clockmaker |
| 39 | Echo of the Duke | Fangela (519) | 4-5 | Hunt | re-clear Cloud Nine Spire |
| 40 | Rival Bakers | Scorchmello (669) | 4-5 | Reconcile | with Crumbelina (73) → friendly bake-off |
| 41 | Squeak the Bat | Biscuitfang (820), hellpup | 4-5 | Pet | the bat by the Lake of Fire ("I bit someone. Sorry.") |
| 42 | Wall Art | Stencilla (565) | 4-5 | Photo | her hearts on the Wall gate before it falls |
| 43 | Encore! | Riffmaw (511), doom guitarist | 5 | Visit | the Unity Stage |
| 44 | Frog Wedding | Confetti (62), planner | 5 | Match | Warty (912) → Lily Lou (82): the first cross-border wedding |
| 45 | The Big Picture | Shutterbug (46) | 5 | Photo | the river from the bridge, "no Wall in the frame" |
| 46 | Potluck Planner | Mother Nox (778) | 5 | Fetch | 2 Soul Soup |
| 47 | Vapula's Apology | Lord Vapula (999) | 5 | Deliver | apology flowers → Mayor (Vapula's only "scene") |
| 48 | Explorer's Club | Scoutmaster (15) | 4-5 | Visit | the Hellvile imp market |

**Shape:** every quest is a single step with no follow-ups, no cutscene, and no change to the plot or the town except relationships and friendship. Shared generic lines carry all target-side reactions. Recurring cross-quest threads are rare (Rivet ↔ Anvil, Charlotte ↔ Nana, Morguelet ↔ Pip, Shutterbug's two photos).

---------------------------------------------------------------------------------------------------
## 9. Credits (`credits.c`)

* 500 ranks. Each pairs Hillvile citizen `n` with Hellvile citizen `500+n`, one pair every 11 ticks at 6 units per tick, led by the player and pet (`credits.c:14-20, 133-156`).
* Total length is 6,162 ticks, **about 3.4 min**. Hold A for 6x speed; START skips to the end.
* Ten cast members cheer and bounce at the roadside (157-171). The dead and ghosts march translucent "in loving memory" (127).
* 18 title cards fade at the top (31-50). The opening cards say "A Dippinn Productions game" and "Made with love, plush stuffing and an alarming amount of vapor".
* A name ticker shows "A ♥ B" with "n/1000" (174-199). Confetti and 2 fireworks (258-275). End card: "Everyone is together in Hellvile Park." (235-244).
* It is rendered with the real `plat_overworld` on `MAPID_PARK_UNITED`, using the overworld's billboard array.
* On return: `BEAT_DONE`, the united park, NG+ and title-theme unlocks, then `CUT_FREEPLAY` and `L_done` (`story_script.c:1127-1135`).

---------------------------------------------------------------------------------------------------
## 10. Audio cues (`audio.h`)

* **19 songs** (`audio.h:9-18`): TITLE, HAHALAND_DAY, HAHALAND_NIGHT, CREATOR, SHOP, CHURCH, PET, HELLVILE, EXORCISM, WORKSHOP, INVASION, HELL_BREACH, BOSS, VICTORY (stinger), SAD, FESTIVAL, CREDITS, VAPE_LOUNGE, OMINOUS. They are MML with 4 channels (lead, harmony doubling as the voice channel, bass, noise), played by a 30 Hz sequencer. `audio_music_once()` plays a stinger and resumes the previous song.
* **35 SFX** (`audio.h:20-26`). The story uses BLIP, PICKUP, SPARKLE, SQUEAK, DEMON_ROAR, DOOR, ERROR, HOLY, SELECT, HAPPY, SPLASH, CANCEL, and in cutscenes EXPLODE, DEMON_ROAR, EGG_CRACK, POSSESS, DEMON_DIE, CONFETTI. It never uses LOVE, SAD, BELL, SPLAT or BUBBLE, which are natural comedy stingers.
* **Voice babble** runs per typed character (`audio.h:90-103`).
* **Song use in the story:** songs unlock to the jukebox per finished chapter (`story.c:89-95`). Cutscene music is set in `cs_enter` (`cutscene.c:92-105`) and overworld music in `ow_music_for_map`.
* The story never changes music *within* a conversation. There are no stings on punchlines and no silence beats.

---------------------------------------------------------------------------------------------------
## 11. How the story connects to the other systems

### 11.1 Combat and Hell Breach
* **Hell Breach 1, the Vape Cellar:** `story_push_combat(MAPID_VAPE_CELLAR, …)` (`story_script.c:584-587`) pushes `SCENE_COMBAT` directly. Win: 100 H$, 3 Scrap, `L_cellar_won`, `CUT_ERUPTION`.
* **Hell Breach 2, Cloud Nine Spire:** (`594-597`). Win: 300 H$, Gate Sigil, `L_spire_won`, `CUT_VAPULA_DOWN`. Vapula has a purple boss bar and a "LORD VAPULA IS DEFEATED!" toast (`combat_hud.c:494-510`, `combat.c:504-508`). **The story does not use `SCENE_BREACH`'s boss intro cards or quips** (`levels.c:53-89`, `lvd_boss_intro`), so Vapula never speaks in the story fight.
* **Invasion encounters:** walk into a demon pack in C3_NESTS (`overworld.c:668-676`), then `script_nest_encounter` (`story_script.c:668-676`) runs combat on the invaded overworld map itself (`combat_start_at`). A cleared nest pays 40 H$ and 2 Scrap.
* Side hunts: #21 and #26 on the invaded map, #39 in the Spire.
* **Hell Breach levels 3-7** (Sugar Rush Sewers, Plushie Factory, Graveyard Shift, Bone Cathedral, Hellvile Underbelly) and their named mini-bosses (Gumdrop Gutterking, Foreman Fluffnuts, The Weeping Widow "She Lost Her Plushie in 1887", Bishop Bonebottom, Gatekeeper Grumbleguts) **exist only as activities** (Arena or Hellvile, `levels.c:662-683`). The plot never references them: a ready-made pool of set pieces and characters.
* Combat maps can fire story triggers (`combat.c:1123`). The combat HUD shows `story_objective()` (`combat_hud.c:519`).

### 11.2 Pet
* At C1_HOME: `SCENE_PET` arg 0 is the hatch tutorial, which counts as done once `pet_exists()`. Then C1_HATCH.
* The pet follows you on the overworld, appears in the credits, and gets its name into text via `@E`.
* The pet has **no story role after hatching**: no scenes, no reactions, no evolution beats. The trailer's best runner gag (the Doomogachi "beep.", tiny horns, horned poop) is absent from the game's story.

### 11.3 Exorcism
* Practice: `SCENE_EXORCISM` arg -1 on Mr. Huggins (C2_FUZZ). The real thing: Steamy (C2_EXORCISE), Hattie and Grampy (C3), Lady Smog (C4), plus 4 side quests.
* Three failures lose the soul (`resident.h:157`). The odds are shown in Fuzz's prompt (`story_script.c:731`).
* The ritual (`exorcism.h`) has three parts: prayer rhythm, holy-water aim, and a true-name pick from 4 Hellvile citizens using clues.
* **The possessor is a hash-picked Hellvile citizen** (`sim_soul.c:58-90`), not authored. The "@D is a real person from the other side" reveal (`L_steamy_saved`, `story_script.c:215-221`) is therefore random and never revisited in chapter 4.

### 11.4 Welding
* At C2_RIVET: a binary choice (Bubble Wand cute, or Pump Shotgun brutal) calls `give_weapon` (`story_script.c:542-559`, quality 72, heart or skull charm), then `SCENE_WORKSHOP`.
* In C3, the first visit to Rivet's workshop gives an "upgrade" (just the workshop scene).
* After that, the Armory is in the pause menu, and Weld & armory is at the workshop and forge.
* The workshop has 8 Rivet quips (`workshop.c:32-41`). One of them, "Back in Hellvile I welded gates.", is an unused story seed.

### 11.5 Free play
* After `BEAT_DONE` the story save keeps roaming the overworld. The 5 post-game side quests unlock, and `side_objective` drives the HUD.
* The free play island (a separate town) can push `SCENE_STORY` with `STORY_ARG_VISIT_{HAHALAND,HELLVILE,PARK}` (`fp_island.c:545-548`): a stroll with no plot, no side quests and no saves (`overworld.c:259-267`).
* Unlocks: United Park, Hellvile Visits, New Game+ (`unlocks_story.c:19-21`). **NG+ has no implementation.**
* The title screen calls `story_begin()` (`fp_title.c:62-71`).

---------------------------------------------------------------------------------------------------
## 12. The current plot, beat by beat

(L_* = line block in `story_script.c`; "→" = what happens next)

### Chapter 1, "Welcome to HaHaland" (11 beats)

1. **C1_ARRIVE.** The chapter 1 card, then `CUT_ARRIVAL` (a pink bus over candy hills: "Population: 500 lovely citizens. And now, @P!") (`overworld.c:335-343`, `story_script.c:1037-1039`).
2. **C1_MAYOR.** Mayor Gigglesworth ("Mayor Giggs") at the fountain, `L_mayor_hello` (62-74, 10 lines). Brimstone Bites ("It's ironic!"), the two sides of Hellvile Park, "We don't talk about the other side. Hee. Hee." The key is with Nana.
3. **C1_KEY.** Nana Pudding, `L_nana_key` (76-87). She baked the key into a muffin, then hands over a Doomogachi egg ("The last one hatched into a little demon. Ha! Kidding. Mostly.").
4. **C1_HOME.** The cottage with the pink door (4,7): `CUT_HOME`, then `L_home`, then `SCENE_PET` hatch (921-931, 1017-1019, 1055-1061).
5. **C1_HATCH.** `L_hatched` (95-100). "@E is here!" A kid is crying outside.
6. **C1_PIP.** Pip lost Mr. Flops at the beach, `L_pip_lost` (102-109): "My eyes are just doing a rain."
7. **C1_BUNNY.** A beach trigger finds Mr. Flops, "damp, but still smiling his stitched little smile" (111-115, 829-836).
8. **C1_BUNNY_RETURN.** Pip's thanks (117-123): a skull sticker, "make friends with EVERYONE". Reward 30 H$.
9. **C1_FRIENDS.** Make 3 new Hillvile friends by talking to anyone, then `L_three_friends` and a text from Nana (125-129, 1198-1200).
10. **C1_CUPCAKE.** Nana's cupcake goes to Miss Lulla (131-145). "I'm going to cry. I'm crying." The fog by the Wall "giggles at night".
11. **C1_WALL.** The fog trigger at the Wall (55,24) and a choice, then `CUT_FOG` (red eyes, "come and PLAY with us"), then `L_fog_after`. The Mayor drags you away: "It's just... weather. Hellvile weather." → C2 (147-153, 838-848, 1020-1023).

### Chapter 2, "Something in the Vapor" (7 beats)

12. **C2_VAPOR.** The chapter 2 card, then "A few days pass", and Cloud Nine Vapor opens by the Wall (158-162). Vex: "I'm from out of town. WAY out." The vape choice: decline or take one puff (the puff adds +15 corruption and gives a Vape Pen; **no later effect**) (164-184, 511-524).
13. **C2_WEIRD.** "Ask around" 3 times; the weird topics ("Everything tastes like grape").
14. **C2_SCREAM.** Steamy the bath-house keeper is possessed in the plaza, `L_steamy_possessed` (191-197): "Steamy isn't home right now. We moved in." The Mayor: "Get Father Fuzz!"
15. **C2_FUZZ.** Father Fuzz ("Stuffed with love and a little bit of buckwheat"). Practice ritual on Mr. Huggins (199-211).
16. **C2_EXORCISE.** Exorcise Steamy (odds shown). Saved: "Why do I taste grape?" and the @D true-name reveal ("a citizen of Hellvile"). Lost: "When you are ready, we can lay him to rest." (215-228, 1071-1085).
17. **C2_RIVET.** Rivet the runaway imp: "Don't make it weird." Lord Vapula is "cooking something big". Cute or brutal weapon, then the workshop, then "Your Vex guy slipped into his cellar with a glowing crate" (230-256, 561-582).
18. **C2_CELLAR.** The cellar hatch leads to **Hell Breach 1**. Won: "Too late, Hillvile. The big cloud is already rising. Kisses! - V". The ground shakes, then `CUT_ERUPTION` (260-271, 1119-1121).

### Chapter 3, "Hell Invades" (3 beats)

19. **C3_ERUPTION.** The cutscene: the cloud swells, grins, and "tears the gate WIDE OPEN. THE DEMONS OF HELL ARE HERE." Hattie Haha and Grampy Bean are possessed. The map switches to HaHaland-Invaded, then the chapter 3 card (1162-1167).
20. **C3_NESTS.** `L_invaded` (276-283): the Mayor, "The sky RIPPED", everyone is hiding in the church. Clear **4 nests** (plaza, beach, park, graveyard: combat on the map) and **exorcise Hattie and Grampy** (generic narrator lines only, 317-333). Rivet's upgrade nudge appears when 2 nests remain (290-298). `L_all_nests` (300-304): "Lord Vapula himself waits at the top. Go with love."
21. **C3_SPIRE.** "COME UP, LITTLE SNACK." (306-310), then **Hell Breach 2**. Won: "Lord Vapula, Duke of the Ninth Cloud, lets out one last enormous wheeze...", then `CUT_VAPULA_DOWN` ("...and deflates. Pfffffffff. With a sad little squeak.") (312-315, 1122-1124).

### Chapter 4, "Hellvile Park" (6 beats plus the ending)

22. **C4_GATE.** The town heals and Lady Smog becomes possessed. HaHaland again, then the chapter 4 card, then `L_c4_open` (338-345). Fuzz: "Were they ever truly our enemies?" Rivet: "My mum's over there." Use the Wall gate (55,30) to reach Hellvile (1170-1178, 894-902).
23. **C4_PRISONERS.** Tallyworn: "3,652 days pumping vapor at the gate… They think Hillvile folks are monsters." (347-353). Befriend 5 Hellvile citizens, then "a hero from Hillvile is in town, and they're... NICE?" (355-359).
24. **C4_HELP.** Mother Nox at her chapel ("popped Vapula like a balloon"; potluck host) asks for 3 favours (361-370):
    * **Sootkin** wants Hillvile flowers for the secret sunflower under the gate (372-383).
    * **Lady Smog** needs exorcising ("I haven't tasted air in eleven years. It's DELICIOUS.") (385-400).
    * **Anvil** at the forge, and Rivet: "...Mum?" "...RIVET?!" "They hug like two vices closing. Sparks fly." "It's a REALLY good spoon." (402-410).
    * Then `L_help_done` (412-416).
25. **C4_MAYOR.** Fetch the Mayor from HaHaland (`L_mayor_cross`: "You walked through demons for us. I can walk through a gate."), then **the summit** in Hellvile (425-433): "I brought cupcakes." "...I brought bone soup." "...I LOVE soup." "Then I believe we have nothing left to build a Wall for." Then `CUT_WALL_FALLS`.
26. **C4_FESTIVAL.** The united park: "Somebody started a festival. Everybody came." Chat with 4 people (435-439, 1206-1208).
27. **C4_STAGE.** The Unity Stage speeches, `L_stage` (446-454): the Mayor, Nox, Pip ("And Mr. Flops!"), Fuzz ("Even you, Vex"), Vex ("plain water vapes now. They're terrible."), Rivet ("Hit it!"). Then **the credits parade**.
28. **DONE.** `CUT_FREEPLAY` ("THE END… of the beginning!"), then `L_done` (456-460). Free roam with post-game side quests.

---------------------------------------------------------------------------------------------------
## 13. Named characters and roles

**Story cast**:

| Character | id | Who | Role now | Lines (main / idle) |
|---|---|---|---|---|
| You (@P) | `DOOMO_PLAYER_ID` | creator-made; default "Sunny", a bunnykin | silent hero | **2** / — |
| Doomogachi (@E) | pet | hatched at C1_HOME | follows you | 0 |
| Mayor Gigglesworth ("Mayor Giggs") | 0 | cherub mayor, "Hee. Hee." | guide, comic coward, summit | 23 / 6 |
| Nana Pudding ("Nana Puddin") | 1 | sheepkin baker | key and egg, cupcake | 10 / 6 |
| Pip + Mr. Flops | 42 | bunnykin kid + plush | first quest, stage | 8 / 5 |
| Father Fuzz | 77 | plushie priest | exorcism mentor | 17 / 5 |
| Mr. Huggins | (plush) | Fuzz's dummy | practice target; a grave sign (`story_places.c:337`) | 0 |
| Rivet | 13 | runaway imp welder (HELLBORN, district 0) | weapons, Anvil reunion | 17 / 4 |
| Dr. Bloom | 111 | ghoul doctor, cold hands | healer place; **no plot beat** | 0 / 5 |
| Vex | 666 | Cloud Nine owner, infiltrator | ch2 antagonist, then vanishes; one stage quip | 6 / 3 |
| Lord Vapula | 999 | Duke of the Ninth Cloud | final boss; **no spoken lines**; post-game plush quest | 0 / 1 |
| Steamy | 2 | bath-house keeper | first possession | 3 / 2 |
| Miss Lulla | 36 | daycare | cupcake recipient, fog hint | 4 / 3 |
| Hattie Haha | 71 | comedian ("makes Hellvile folk snort-laugh") | ch3 possessed #1; **no authored lines** | 0 |
| Grampy Bean | 88 | grandpa, tomatoes | ch3 possessed #2; **no authored lines** | 0 |
| Tallyworn | 712 | gate laborer counting days | first Hellvile voice | 4 / 2 |
| Mother Nox | 778 | Hellvile high priestess, potluck | quest hub, summit | 13 / 2 |
| Sootkin | 525 | gate laborer, secret sunflower | help task 1 | 4 / 1 |
| Lady Smog | 502 | loyalist courtier | help task 2 (exorcism) | 2 / 1 |
| Anvil | 595 | blacksmith, Rivet's mum | help task 3 | 3 / 1 |
| Haze | 709 | Cloud Nine clerk who hated the smoke | one idle line | 0 / 1 |

**Place keepers with Talk options** (`story_places.c:49-72`): Mintworth (56), Quillby (123), Maestro Mew (3), Squeaker (64), Keysie (21), Gutterworth (505), Bargainia (545), Fangela (519), Capt. Whisk (48), Barnaby (768), Taco Yaki (128). Their Talk falls through to generic small talk.

**Side-quest people:** see §8. About 45 more named citizens appear there.

**Mentioned only:** St. Cuddles, The Fluffones (band sign), Sir Whiskers, Mr. Biscuit, Squeak the bat, the joke graves (Mr. Wiggles, Grandpa Puff's moustache, the Great Cake of '89, Fluffy the cloud, Count Snoozula).

---------------------------------------------------------------------------------------------------
## 14. Weak spots: what is thin, rushed or flat

### 14.1 Structure and length
* **Chapter 3 is three beats.** It amounts to four interchangeable nests, two silent exorcisms and the boss. The tonal climax (cute town turns to Doom) gets one 5-line Mayor speech (`L_invaded`).
* **Chapter 2 rushes.** The vape shop opens and someone is possessed within about 20 lines. The "people get weird" phase is a 3-chat counter.
* **Counters are padding, not story:** C1_FRIENDS, C2_WEIRD, C4_PRISONERS, C4_FESTIVAL (§0.3).
* **The game never uses time.** Nothing is gated on days, nights or the calendar ("A few days pass" is narration only). The day/night cycle, sleep and the life sim exist but never shape the plot.
* **Quest variety:** every main beat is "go to X, talk", except 2 breaches, the nests and 4 exorcisms. There are no escorts, chases, stealth, investigations, puzzles, timed events or minigame-in-story moments, although fishing, cooking, concerts, dreams, minigames and Breach 3-7 all exist.
* **Side quests are 49 one-steppers.** None chain, none have a scene, and target reactions are generic. Some titles mismatch their content (#9, #16, #26).

### 14.2 Heart (emotional beats that are set up, then dropped)
* **Rivet and Anvil** is the best beat, but it is 6 lines. The setup is two throwaway lines in ch2 plus side quest #18. Anvil sets a dinner place for Rivet every night (her roster bio); that is never shown.
* **Steamy's loss** is acknowledged once (`L_steamy_lost`). Nothing in the town remembers it: no funeral or grave beat in the story, although `SCENE_GRAVEYARD` exists.
* **Hattie and Grampy** (comedian, grandpa) have no lines, possessed or freed. Win or lose, they get the same generic narrator text (`L_c3_saved` / `L_c3_lost`).
* **The prisoners twist** (Hellvile's citizens were victims) is told in 4 lines (`L_c4_open`, `L_tally`), never shown. The random @D possessors are the natural payoff: meeting the citizen whose name you shouted at Steamy. They are never revisited.
* **The Mayor's fear of the other side** ("We don't talk about the other side") is the game's emotional spine. It resolves in a single soup joke. There is no backstory: why was the Wall built? (No lore at all on the Wall's origin.)
* **The player has no arc** and almost no voice (2 lines). The choices (vape puff, cute or brutal, peek or back away, exorcise now or later) don't echo anywhere.
* **The pet has no arc** after hatching.

### 14.3 Scary
* The real scares are the fog cutscene (3 panels), Steamy's possession (4 lines), and possessed small talk ("Leave a message after the scream." is the best scary-funny line in the game). The invasion is told, not staged.
* There are no build-ups, false calms, silence, jump scares, or night-time horror beats. The tint, SONG_OMINOUS, SFX_POSSESS and the skull emotes are all available and underused.
* Vapula is a sprite and a narrator quote. No menace, no motive, no taunting during the climb. The level system already has an authored boss quip, "You dare climb MY spire?" (`levels.c:84-88`), but the story fight doesn't show it.

### 14.4 Funny (the jokes are good on paper; the delivery kills them)
* There are plenty of good gags: the key baked into a muffin, "Hee. Hee.", "It's ironic!", "My eyes are just doing a rain", "It's a REALLY good spoon", "...I LOVE soup", Vex's plain water vapes, "Rest in pieces, Mr. Huggins the 1st".
* **Delivery:** the same box, voice speed and portrait face (TALK/NEUTRAL) for every line. There are no reaction shots, pauses before punchlines, SFX stings, screen gags or callbacks, and the cutscenes auto-advance at a fixed ~8-9 s per panel.
* **There are no runner gags.** The trailer's spine is a set of recurring gags (§15) that the game never uses.

### 14.5 Continuity bugs and dead code found while mapping
* `L_help_done` says "Lady Smog is breathing" even if her soul was lost (`story_script.c:412-416`; `CTX_C4_SMOG` sets the flag on loss too, 1095-1100).
* Steam Cleaning (#13) is hidden if Steamy was lost. Nothing else reacts to that loss.
* Lagspike's pronoun: "his chat" in the quest text (`story_side.c:123`) versus "Her chat" in the roster bio.
* "Graveyard Shift" is used for both a ch3 side quest (Fun Fair spiders) and Hell Breach 5.
* Never used: `SF_VEX_REFORMED`, `SF_VAPULA_PLUSH`, `USTORY_PACIFIST` (never granted), `MODE_NEW_GAME_PLUS` (no implementation), `SF_VAPE_SAMPLE` / `SF_WEAPON_BRUTAL` (written, never read).
* Docs and code disagree. `docs/STORY.md` gives the chapter titles as "Something in the Air", "Hell Comes to HaHaland" and "Hellvile"; the code says "Something in the Vapor", "Hell Invades" and "Hellvile Park". The quest count is 46, 49, or "more than 40" depending on where you look. The `town.h:73` comment says "1..3 demo chapters, 4 = done" (it is now 1..4, 5 = done).

---------------------------------------------------------------------------------------------------
## 15. What the trailer does that the game cannot do yet (target: "spastic, elastic" timing)

The trailer (`trailer/`, offline renderer, **float math, `<math.h>`, palette LUTs built at runtime**, cannot be ported 1:1 into core) runs 15 scenes for about 81 s (`trailer/timeline.c:8-23`). Its comedic vocabulary:

| Device | Trailer example | Runtime equivalent today |
|---|---|---|
| a 2-4 frame "flicker" reveal | devil tail behind the splash heart for exactly 3 frames (`s01`); Doomogachi LCD pentagram for 4 frames (`s04`) | none (cutscene panels are at least 8 s) |
| freeze, side-eye, sweat | the heart freezes and side-eyes the audience (`s01`); the logo sweats (`s02`, `s15`) | none (no per-actor expression or emote on cue) |
| record scratch, freeze frame, "!" over every head | `s08` f14 | none (no time-stop; NPC emotes are automatic) |
| deadpan "beep." / "it's fine." / "(cozy.)" tag lines | `s04`, `s07`, `s08`, `s12`, `s15` | possible as text; no beat or pause control |
| snap zoom / extreme close-up | Kevin's third eye (`s05`), the demon "ow. my everything." (`s11`) | none (no camera or zoom; portraits only at 1x in the dialogue box) |
| long hold, then a pop | inhale… hold… FWOOMP (`s07`); "Drip. Drip." silence (`s11`) | none (no silence or hold control, music never ducks) |
| stamp slam + sad kazoo | "EXORCISM FAILED x1" (`s11`) | the exorcism scene has its own result screen; the story has no stamp |
| squash and stretch, overshoot, bounce | letters drop with squash (`s02`), heart BOING (`s01`), consoles high-five (`s14`) | only `ui_pop_scale` (window pop) and `ui_heart_squash`; sprites can't squash |
| a runner character across scenes | the pink party bunny: vape shop → possessed → church → cured at the ending (`s07`, `s08`, `s11`, `s13`) | none (no recurring gag NPCs in the plot) |
| a sequence of gags inside one shot | the Doomogachi eats, poops, pentagram, horns (`s04`); the tail catches fire, the "DAYS SINCE LAST FIRE" sign flips (`s09`) | none (no multi-actor timeline in cutscenes) |
| palette rot | `tk_post_hellify` 0→16 dither creep (`s08`) | overworld tint levels 0-8 (`ow_view_fill`); no cutscene rot |
| shake, glitch, flash, iris, fade | `tk_post_*` (`trailer/tk.h:116-126`) | `ow_shake`/`ow_flash`/fade on the overworld; ad hoc shake in the eruption cutscene |
| plush chibi at sizes 1-3 with expressions | `tk_chibi(..., expr, mouth, bob, flip)` (`trailer/tk.h:80-111`) | `bodyspr_image` (16x24 chibi, 4 dirs x 4 frames), `figure_image` (48x80), `ui_portrait` (11 expressions) |

To match the trailer, the runtime needs:
* a fixed-point **beat/timeline layer** (holds, hard cuts, 1-4 frame inserts, eased moves in integers);
* **per-line portrait expressions, emotes, SFX and shake** in dialogue;
* **NPC and camera scripting** on the overworld (walk-to, hop, turn, emote, pan, zoom on PS1).

All of it must be integer math, and on PS1 it must live in the story overlay group or be data the resident code can read.

---------------------------------------------------------------------------------------------------
## 16. The "dream house" request against what exists

* **Exists:**
  * `PLACE_HOME` door at (4,7) on Hillvile Lane, with a menu of Rest & save, Activities (Peek at a Dream at night) and Leave (`story_places.c:259-286`, `actloc_of` 40);
  * the mailbox gag (`overworld.c:510-513`);
  * the `CUT_HOME` vignette art (`cutscene.c:299-328`: heart wallpaper, wood floor, window, bed, cushion, all drawn with UI primitives and textures);
  * the apartment buildings `PLACE_APARTMENTS` (Hillvile Towers) and `PLACE_HELL_PITS` (Rest & save only);
  * in free play, `SCENE_APARTMENTS` (a 500-window facade browser, `fp_apartments.c:1-16`) shows residents in windows. No interiors.
* **Does not exist:** an interior map or scene, furniture sprites, catalog or items, a placement or edit UI, a home save chunk, home-related quests or rewards. The side-quest unlocks (keepsakes and badges, `USTORY_REWARD_FIRST`+) are natural decor rewards, but they are not wired to anything placeable.
* **Constraints:**
  * `ITEM_COUNT` 63 against a cap of 64: furniture cannot be inventory items and needs its own catalog plus a save_ext chunk (as 'FISH', 'SONG' and 'FPLY' do).
  * `TEX_COUNT` 48 against a cap of 64: 16 texture ids free. Interior textures already exist: `TEX_WOOD_FLOOR`, `TEX_CARPET`, `TEX_WALLPAPER_HEARTS`, `TEX_TILE_FLOOR`, `TEX_WOOD_WALL`, `TEX_PASTEL_BRICK`, `TEX_CANDY_STRIPE`, hell textures.
  * `SPR_COUNT` 54 with at most 224 images (PS1 doc: 194 frames used). No furniture sprites exist; props are tree, lamp, bench, fountain, barrel, skulls, torch, sign, flowers, mailbox.
  * The edit scene can claim the 12 KB `scene_scratch` while running. Only the saved layout costs permanent EWRAM (GBA 7.5 KB free).
* **Renderers:**
  * `plat_overworld` already gives a PS1 2.5D/3D tilted view and a GBA top-down-with-block-faces view of any small cell map, so a cottage interior *map* reuses everything (doors, collision, NPC billboards, props, tint, daylight).
  * A true **isometric GBA view** needs a new 2D scene drawn with `plat_image` and ui primitives in mode 4 (sorted diamond tiles plus sprite stacks), or a new platform hook.
  * On PS1 a decorate scene would be a new overlay group, or join the story group (about 60 KB headroom).

---------------------------------------------------------------------------------------------------
## 17. Test infrastructure

**`core/tests/test_story.c`** (509 lines). Tests and where they start:

| Test | Line | What it covers |
|---|---:|---|
| `story_full_playthrough_reaches_the_credits` | 48 | the whole arc through the real overworld with autowin; A through dialogue and cutscenes, START through the credits; checks key beats, flags, unlocks and maps |
| `story_debug_jump_sets_up_every_beat` | 97 | every beat has an objective and a reachable target |
| `story_overworld_maps_are_valid_and_reachable` | 190 | map validity and reachability |
| `story_every_target_and_npc_spot_is_reachable` | 207 | flood fill on foot |
| `story_side_quests_table` | 236 | `n` between 40 and 50, every kind used, ≥15 Hillvile and ≥10 Hellvile givers, ch1 ≥8 / ch2 ≥6 / ch3 ≥6 / ch4 ≥8, giver and target spots reachable (ch3 HaHaland givers also on the invaded map) |
| `story_side_quests_play` | 288 | quests through the real talk path |
| `story_small_talk_has_something_to_say` | 352 | small talk |
| `story_flags_and_formatting` | 368 | flags and tokens |
| `story_places_and_activities_menu` | 402 | place menus |
| `story_save_round_trip_mid_story` | 430 | save and load mid-story |
| `story_overworld_walks_and_collides` | 453 | movement and collision |
| `story_nest_encounter_fights_real_demons` | 484 | a nest fight with real demons |

Constraints any rewrite must keep, or deliberately change:
* `story_debug_step()` must still be able to play every beat (`script_debug_step`).
* Every target must be reachable.
* The side-quest table limits above.

**Runner scripts** `core/tests/scripts/story_*.txt`: `story_ch1` (137 lines), `story_chapters` (49), `story_cutscenes` (63), `story_invasion` (20). They use runner commands `scene STORY <100+beat>` (or `200+beat` for night), `push CUTSCENE n`, `press`/`hold`/`wait`/`shot`/`expect-scene`. `core/tests/run_scripts.sh` plays every script at **240x160 and 320x240**. A first line `# assert-no-stubs` fails the run if a weak stub runs.

**Other suites:**
* `test_audio.c`: songs parse; SFX within the 48 KB budget.
* GBA `make -C gba test`: libmgba harness, `scripts/smoke.txt`, coresave.
* PS1 harness (`ps1/harness`, needs `--cue` for overlays).

Docs: `docs/STORY.md` (partly stale, §14.5).

---------------------------------------------------------------------------------------------------
## 18. Hooks and constraints that matter for the expansion

* **Where text can grow cheaply:**
  * GBA ROM: unlimited for practical purposes; zero EWRAM if `const`.
  * PS1: text referenced only from overlay code is free up to the slot. Text reached from resident callbacks costs resident RAM (75 KB margin, shared with every other module's growth).
  * A **table-driven script** (data rows with opcodes like say, emote, wait, shake, sfx, move, cam, choice, goto, set flag) interpreted by one resident function would keep most new story bytes in the overlay group or a streamed per-chapter pack, instead of adding resident callbacks.
* **Persistent state:**
  * 28 reserved flag bits plus `SF_31_FREE` and spare bits in 36-39, 42-47, 48-55 and 58-63. Only `SF_MET0+8` (Tallyworn) and `SF_MET0+9` (Mother Nox) are used.
  * Beyond that, add a story `save_ext` chunk. That needs an additive registry slot in foundation-owned `save_ext.h`/`.c`, like `doomo_save_ext_fishcook`.
  * A ch5+ beat list must append to the `BEAT_*` enum ("Append only (saved)", `story.h:74`).
* **Hooks that already exist and are unused:**
  * Breach 3-7 bosses with names and quips;
  * the Dream scene (Peek at a Dream) for nightmare or comedy sequences;
  * Concert/Studio for the stage finale;
  * Cooking (Nana/Charlotte), Fishing (Capt. Whisk/Barnaby), minigames (Whack-an-Imp "Do NOT bonk Father Fuzz");
  * the Graveyard scene (Steamy, Hattie, Grampy, Lady Smog losses);
  * `sim_confess` / relationship edges (pre-seeded cross-border crushes in `core/data/roster/edges.txt`);
  * the 1000-citizen roster traits (`ROSTER_T_SOFTIE`, `LOYALIST`, `VAPER`, `INFILTRATOR`, `KID`, `ELDER`, `NIGHT_OWL`; `roster.h:66-77`) for casting new arcs;
  * portrait expressions (11) and the overworld emote bubbles;
  * `TINT_*` levels and `audio_music_once` stingers.
* **PS1-specific:**
  * Keep story to minigame cuts rare (CD reads).
  * The 3D overworld already does 2.5D: interiors and cutscene "stages" can be small cell maps rendered by `plat_overworld`.
  * Camera control would need a field in `DoomoOverworldView` (today cx/cy only, with a fixed pitch and distance in `ps1/src/overworld.c:24-30`).
* **GBA-specific:**
  * Any new persistent RAM comes from the 7.5 KB.
  * Big per-scene state goes in the 12 KB scene scratch.
  * Top-down overworld blocks (`gba/src/overworld.c`) are the "cheap 3/4" precedent; isometric would be new 2D drawing code in ROM (THUMB), with the cost in CPU, not RAM.
