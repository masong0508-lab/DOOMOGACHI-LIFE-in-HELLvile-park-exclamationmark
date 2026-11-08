# Story Mode, overworld and audio

Owner: story. Code: `core/src/story*.c`, `overworld*.c`, `cutscene.c`,
`credits.c`, `unlocks_story.c`, `audio.c`, `songs.c`; headers
`core/include/doomo/story.h`, `audio.h`; private `story_int.h`,
`overworld_int.h`, `audio_int.h`. Maps: `core/data/maps/{hahaland,
hahaland_invaded,hellvile,park_united}.txt` (compiled by
`tools/mapc.py` into `core/gen/map_*.c`). Tests: `core/tests/test_story.c`,
`test_audio.c`, runner scripts `core/tests/scripts/story_*.txt`.

## Entering Story Mode

| Call | What happens |
| --- | --- |
| `story_begin()` | New story: fresh `TOWN_STORY` town (premade 1000 citizens), then `SCENE_STORY` with `STORY_ARG_NEW`, which starts Chapter 1 (the prologue, the chapter card, the arrival). **CL2 (C2):** a hero nobody has chosen yet (`town_new` calls them "You") gets the creator's hero cards (`SCENE_CREATOR` -3: SUNNY, MARVIN or make your own, "Who are you?") right after the arrival's last caption, "And now, You!", as its punchline. The beat is already `C1_MAYOR` when the cards open, so B on them keeps "You" and nothing else in the flow moves; `.dsc` scripts and `flow.lock` are untouched. Only this title path offers the cards (`ow.hero_offer`); `scene STORY 0` and the debug beats keep their old flow, so the runner scripts that start that way are unchanged. A town with no protagonist at all still opens the creator before the prologue. |
| `scene_set(&SCENE_STORY, STORY_ARG_CONTINUE)` | After `load_game(slot)`: resumes on the saved map, cell and beat. |
| `STORY_ARG_VISIT_HAHALAND / _HELLVILE / _PARK` | Free-play stroll with no plot (Hellvile Visits / United Park modes). |
| `STORY_ARG_DEBUG_BEAT + beat` (`+ STORY_ARG_DEBUG_NIGHT` for 21:30) | Debug: new story town set up exactly as at that beat. Used by runner scripts, e.g. `scene STORY 219`. |

Everything the story pushes (combat, exorcism, pet, workshop, cutscene,
credits, shop, graveyard, options, activities) comes back to
`SCENE_STORY`; the overworld notices it is on top again and resolves the
"pending" sub-scene (`story_resume()`), then restores its map, music and
dialogue style.

## Chapters and beats

| Ch | Beats (`BEAT_*`) | Highlights |
| --- | --- | --- |
| 1 Welcome to HaHaland | C1_ARRIVE, MAYOR, KEY, HOME, HATCH, PIP, BUNNY, BUNNY_RETURN, FRIENDS, CUPCAKE, WALL | Mayor Gigglesworth, the house key, Nana Pudding's egg (`SCENE_PET` hatch tutorial), Pip's runaway bunny, make 3 friends, the cupcake errand, the Wall and the purple fog. |
| 2 Something in the Air | C2_VAPOR, WEIRD, SCREAM, FUZZ, EXORCISE, RIVET, CELLAR | Cloud Nine Vapor and Vex, residents acting weird, the first possession, Father Fuzz's exorcism tutorial (`SCENE_EXORCISM`), Rivet's welding (`SCENE_WORKSHOP`, cute vs brutal starter), Hell Breach 1: the Vape Cellar (`SCENE_COMBAT`). |
| 3 Hell Comes to HaHaland | C3_ERUPTION, NESTS, SPIRE | The gate erupts, `MAPID_HAHALAND_INVADED` with metal music, 4 demon nests (walk into one: combat on the same map), exorcise the possessed, Cloud Nine Spire and Lord Vapula. |
| 4 Hellvile | C4_GATE, PRISONERS, HELP, MAYOR, FESTIVAL, STAGE | Through the Wall, Hellvile's prisoners, redemption / befriending / exorcism, the Mayor crosses over, the Wall comes down, the festival in `MAPID_PARK_UNITED`. |
| End | DONE | `SCENE_CREDITS`: a parade of all 1000 citizens by name plus the dev credits, then free play (United Park, Hellvile Visits, New Game+ unlocked). |

`story_debug_jump(chapter, beat)` sets up any beat (flags, items, pet,
weapon, possessions, map, position). `story_debug_autowin(1)` makes
combat / exorcism / pet / workshop resolve instantly (tests only);
`story_debug_step()` performs the obvious next action, so a test can play
the whole story (see `story_full_playthrough_reaches_the_credits`).

## The overworld (`SCENE_STORY`)

* `plat_overworld` 3D view with collision (walls, water, lava, steps of
  at most 3 height units), props, NPC bodies, the pet following you.
* A talks / opens doors (or push into a door for 6 ticks), START pauses
  (Resume, Quest Log, Boreagotchi, Armory, Map, Options, Save, Title
  Screen), SELECT toggles the minimap, B runs.
* **Pause panel (CL2, C11):** a help line under the rows says what the
  highlighted row does. A row that cannot be used yet is drawn greyed
  (`ui_button_off`) and its help line gives the reason instead: the pet
  ("hatches soon, in Chapter 1") until it hatches, the Armory ("Rivet
  welds you a weapon in Chapter 2") until `SF_WEAPON_CHOSEN`; A on it
  only buzzes (no toast). One table, `ow_pause_row()` in `overworld.c`,
  answers both the drawing and the input. The 320x240 footer reads
  "♥1 friend" / "♥N friends" and "★done/total side quests". The 240x160
  panel is 148 px high (was 138) to fit the help line.
* Doors are wall cells tagged `STORY_TAG_DOOR + PLACE_*`; walk-on events
  are `CELL_TRIGGER` cells tagged 128+ (`STORY_TAG_EVENT_*`, secrets
  135..142). NPC wander zones are `THING_NPC` (arg = zone), demon nests
  are `THING_SPAWNER` (arg 0..3) on the invaded map.
* Objective pill + goal arrow, clock and money (H$), region banners,
  bubble markers ("!" main, "?" side, check = ready to hand in).
* **Wayfinding (clarity C7, CL6).** Every story objective says where:
  a compass word from the plaza fountain ("(NW)", "(park, S, @0/3)"), a
  landmark the player stands at ("(here)", "(home)", "(plaza)"), or
  "follow the arrow" when it has several targets. `core/tools/
  story_wayfind.py` (in `story-check`) refuses an objective without one,
  or one that needs a third line in the pill at 240x160; the test
  `clarity_c7_every_objective_names_where_and_fits_the_pill` decodes all
  59 and wraps them with the real font. The goal arrow is a filled arrow
  at the screen edge, never in the HUD band (its top stays at y >= 30,
  `OWFX_ARROW_TOP`), drawn in spans; it pulses (bobs and flashes white)
  for 60 ticks after the objective changes and on arrival. The mini-map
  (SELECT, named in the first controls hint) and the quest log's Map tab
  draw "you" as an always-on ringed dot (`owfx_you_marker`); the Map tab
  names the goal ("Goal: Nana Puddin, Brimstone Bites": who, then the
  building whose door is within 3 cells, else the region, else the map;
  `story_goal_place`) and says "SEL while walking: mini-map".
* **One name per thing (clarity C16, CL6).** The HUD and menus use the
  map's plain place names; the Hillvile / Hellvile pun stays in dialogue.
  The park region of HaHaland is "The Park" (it was "Hellvile Park -
  Hillvile Side" a second after the "HaHaland" banner), the bakery is
  always Brimstone Bites, the Hellvile flats are Hellvile Pits ("Hellvile
  Pits Flat" in the house), the arena is the Demon Arena (map title,
  island row "Enter the Demon Arena"), the free-play park is "The Park"
  (the island itself is Hellvile Park) with "Walk around HaHaland", and
  money is H$ on the story HUD too. `story_wayfind.py` greps the C string
  literals for the retired aliases.
* Day / night from the town clock; Hellvile is clamped to dusk, the
  invaded town to red sky, the Wall area gets a purple haze in ch1-2.
* Small talk is generated from each resident's catchphrase, likes, job,
  bio and the current chapter (`story_small_talk`).

## Buildings and activities

Every door is a `PLACE_*` (`story_place_name`). Entering one runs the
plot first, then side quests, then the place menu: Talk, Buy (mini shop),
Weld & armory, Rest & save, Get patched up, Browse every shelf
(`SCENE_SHOP`), Visit the graves (`SCENE_GRAVEYARD`), **Activities** and
Leave. Activities come from `activities_at(location, ...)` filtered by
`ACTF_STORY` and `min_chapter`; the row is only offered where some Story
Mode activity is filed for the place (`story_place_has_activities`), and
while all of them are locked (chapter, unlock, hour) it says "Coming
soon!". Town Hall has none since clarity C9 moved Music Studio to the
Concert Hall and Expand the House to the homes, so its menu is Talk and
Leave. Launched activities return to the overworld.

## Side quests

49 quests (`story_side.c`), 12 / 10 / 10 / 12 by chapter plus 5
post-game, from named citizens of both districts. Kinds: fetch, deliver,
find, photo, visit, lost pet, matchmaking, exorcism request, cross-border
reconciliation, mini-boss hunt (combat on the invaded map or the Spire).
Rewards: H$, items, and an unlock (keepsake badges, the bunny-ears hat).
The quest log (`SCENE_QUESTLOG`, tabs Story / Side / Items / Map /
Friends) tracks them.

## Flag map (`DoomoStoryState.flags`, 256 bits)

| Bits | Use |
| --- | --- |
| 0..31 | plot one-shots (`SF_*` in `story_int.h`) |
| 32..39 | demon nests cleared (ch3) |
| 40..47 | ch3 exorcisms |
| 48..63 | met / tutorial flags |
| 64..163 | side quest states, 2 bits each (`STORY_FLAG_SIDE_FIRST`, up to 50 quests: locked, active, ready, done) |
| 164..195 | secret sparkles: HaHaland 0..7, Hellvile 16..23, park 24..31 (`STORY_FLAG_SECRET_FIRST`) |
| 196..227 | places visited (`STORY_FLAG_PLACE_FIRST + PLACE_*`) |
| 228..255 | reserved |

## Unlocks (ids 608..735, `unlocks_story.c`)

608..610 modes (United Park, Hellvile Visits, New Game+), 611..629
jukebox songs, 636..642 cutscenes, 644..654 achievements (chapters, all
side quests, explorer, secrets, pacifist, friend of all, starter weapon),
660..676 side quest keepsakes. Songs and cutscenes are granted quietly
as they are heard / seen; chapter ends show one toast.

## Audio

* 30 Hz sequencer, 4 channels through `plat_tone`: 0 lead, 1 harmony
  (also voice babble), 2 bass, 3 noise drums. Songs are MML strings in
  `songs.c` (dialect documented in `audio_int.h`), 24 instruments with
  ADSR, delayed vibrato, arpeggios and sweeps. Cute songs are pulse-led,
  the doom family (Invasion, Hell Breach, Lord Vapula) saw-riff metal.
* 19 songs: Title, HaHaland Day / Night, Creator, Shop, Church, Pet,
  Hellvile, Exorcism, Workshop, Invasion, Hell Breach, Boss, Victory
  (stinger), Sad, Festival, Credits, Vape Lounge, Ominous.
* `OPT_MUSIC_VOL` scales (0 mutes); `audio_music_once` plays a stinger
  and returns to the song.
* Every `SFX_*` is procedural signed 8-bit PCM generated at boot, all
  within `AUDIO_SFX_BUDGET` (48 KB).
* Voice babble: one blip per syllable from the 5 voice sliders (pitch,
  speed, quality, intonation, accent), honouring `OPT_VOICE_BABBLE` / volume.

## Budgets (GBA build, `-O2 -mthumb`)

Code + const ~130 KB, .data 10 B, .bss ~10 KB for the whole module.
Maps ~127 KB of const cells. Per tick: NPC update over at most 40 NPCs,
billboards capped (`OW_BB_MAX`), the sequencer reads a few bytes per
channel.

## Runner scripts

`story_ch1` (arrival, Mayor, objective, pause, quest log tabs, minimap),
`story_newgame_hero` (CL2: title -> New story -> the hero cards after "And now, You!" -> MARVIN, the pause panel's greyed rows and help line),
`story_chapters` (one shot per chapter / map, day and night),
`story_cutscenes` (every cutscene and the credits parade),
`story_invasion` (a nest encounter into first and third person combat).
