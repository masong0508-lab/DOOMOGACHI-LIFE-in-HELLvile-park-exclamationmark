# DOOMOGACHI speed audit 2: every scene the first audit did not measure (measure and plan)

Date: 2026-11-08. Tree: `origin/claude/nice-allen-gr6czm` at `d875604` (SP1, SP2, SP4 landed; SP3, SP5-SP8 still
landing) plus the measuring commit of this audit, `d30755a` (harness scripts and tools only, no game code). Owner goal, DECISIONS.md
"SPEED WAVE 2": every scene other than combat at a **steady 30 fps with no hitches** on GBA and PS1 (and Dreamcast
as it comes up). The scenes SP1-SP8 own (island hub, town hall / life menus, portraits, dialogue, concert,
exorcism, pet, studio SONG..PERFORM, combat) are not re-planned here.

## 0. How it was measured

| platform | build | runner | numbers |
|---|---|---|---|
| GBA | `make -C gba -j2 rom` (32 MB ROM, 4,534,084 B used) | libmgba 0.10.2, `make -C gba test-speed2` (new) | per window: logic ticks/s, drawn fps, CPU work per drawn tick (avg / max), **the presented frames by vblanks taken** (new harness `gaps` line: 2 = 33 ms, 3 = 50 ms, 4+ = 67 ms+), frames over 33 / 50 ms, the worst frame, the load (scene command time + first drawn tick); `make -C gba profile-speed2` (new) for a PC profile after every window |
| PS1 | `cmake --build --preset ps1 -j 2` (disc + `OVL.BIN`) | PCSX-Redux, `ps1/harness/speed2.sh` (new): the same scripts converted by `tools/speed2.py ps1` | fps (= logic), frames by vsyncs (the SP1 histogram), frames over 33 / 50 ms, worst frame, the overlay loads (`ovl:` lines) before the window |
| Dreamcast | `make -C dc -j2 elf` + stripped ELF | Flycast core, `dc/harness/speed2.sh` (new): converted by `tools/speed2.py dc` | logic ticks/s per window and every tick whose work was over 33.3 ms (dcrun `work:` lines); per group the worst tick |

The scripts: `gba/harness/scripts/speed2_{fair,fishcook,library,action,dream,house,studio,creator,life,story}.txt`,
216 windows. Each window: `seed 1` + a scene command (or the presses of a real flow), a settle, then `fps start` /
`wait 90..130` / `fps GROUP: NAME`; GBA with `sync on`, so every GBA number repeats exactly. Screenshots were checked
for every window (contact sheets in `logs2/`).

Reading the numbers:

* **GBA "work" vs what the player sees.** A tick of 34-38 ms of work can still flip on its second vblank when it ends
  inside the vblank lines, so "work > 33.4 ms" over-counts. The new `gaps` histogram (from the mailbox's `drawn`
  word) is what reached the screen; the >33 / >50 columns come from it. A scene drawn at exactly 15.0 or 20.0 fps
  is a steady 4- or 3-vblank frame (60-67 ms or 40-50 ms of work every tick).
* **GBA logic** is 30 ticks/s unless a frame takes over two ticks of time; logic < 30 means slow motion.
* **PS1** runs one tick a frame; PCSX-Redux is pessimistic for CPU-bound code (1.5-2x, docs/PS1.md). A single `3` at
  the first frame of a window is the lockstep resume and is not counted.
* **Dreamcast:** Flycast is not cycle-accurate; the work times are the game's own timer in emulated time.
* **"load ms"** (GBA) is the scene command's own time plus the first drawn tick. Scenes entered with a *demo* arg
  (`LIBRARY 256`, `DREAM -2`, `STUDIO -107`, `COOKING 129`, `HOME 3`) move the premade citizens in first: their
  1.7-2.0 s loads are the debug setup, not the game. The real loads are the press-driven windows ("the load").

## 1. Results: GBA (every window)

**Summary: 216 windows: 76 ok, 18 with hitches, 122 MISS** (4 of them combat reference windows, out of scope). PS1: 9
windows MISS on speed (credits x2, Prize Wheel x2, Breach card, chapter card 2, workshop, creator try-on, busy house
layout), 6 with single hitches, the Dodge hang and 3 load / save freezes; everything else 30. Dreamcast: only the house
(DC8 pending), the credits and the save / load frames miss.

Columns: logic ticks/s, drawn fps, CPU work per drawn tick (avg, max), the presented frames by vblanks taken
(2 = 33 ms, 3 = 50 ms, 4 = 67 ms, ...), frames over 33 ms and over 50 ms, the worst frame, the load (see 0).
Window lengths: 90-130 ticks for scenes, 20-30 for a cursor move / tab change.

**fair**

| window | logic t/s | drawn fps | work avg ms | work max ms | frames by vblanks | >33 ms | >50 ms | worst frame ms | load ms | verdict |
|---|---:|---:|---:|---:|---|---:|---:|---:|---:|---|
| fair: catch title | 30.0 | 15.0 | 59.0 | 60.0 | 4:45 | 45 | 45 | 67 | 104 | MISS |
| fair: catch play sweet | 30.0 | 30.0 | 27.7 | 29.3 | 2:130 | 0 | 0 | 34 | 91 | ok |
| fair: catch results | 30.0 | 30.0 | 27.6 | 30.6 | 2:90 | 0 | 0 | 34 | - | ok |
| fair: catch play hellfire | 30.0 | 29.5 | 31.0 | 36.8 | 2:125 3:3 | 3 | 0 | 50 | 88 | hitch |
| fair: whack title | 30.0 | 15.0 | 58.8 | 60.2 | 4:45 | 45 | 45 | 67 | 119 | MISS |
| fair: whack play sweet | 30.1 | 15.1 | 57.5 | 63.4 | 3:1 4:64 | 65 | 64 | 67 | 113 | MISS |
| fair: whack play hellfire | 29.7 | 15.1 | 61.5 | 69.7 | 3:4 4:59 5:3 | 66 | 62 | 84 | 108 | MISS |
| fair: dodge title | 30.0 | 15.0 | 60.6 | 61.8 | 4:45 | 45 | 45 | 67 | 115 | MISS |
| fair: dodge play sweet | 29.9 | 22.5 | 38.6 | 45.7 | 2:34 3:64 | 64 | 0 | 50 | 85 | MISS |
| fair: dodge play late | 30.1 | 27.1 | 33.5 | 59.3 | 2:92 3:24 4:1 | 25 | 1 | 67 | - | MISS |
| fair: dodge play hellfire | 30.0 | 20.5 | 42.2 | 48.2 | 2:8 3:81 | 81 | 0 | 50 | 82 | MISS |
| fair: gallery title | 30.0 | 15.0 | 58.4 | 60.3 | 4:45 | 45 | 45 | 67 | 101 | MISS |
| fair: gallery play sweet | 19.4 | 11.0 | 85.1 | 88.9 | 5:42 6:32 | 74 | 74 | 100 | 150 | MISS |
| fair: gallery play hellfire | 18.5 | 10.5 | 87.3 | 94.6 | 5:22 6:52 | 74 | 74 | 100 | 152 | MISS |
| fair: derby title | 30.0 | 15.0 | 59.6 | 61.1 | 4:45 | 45 | 45 | 67 | 138 | MISS |
| fair: derby race sweet | 30.0 | 15.0 | 60.3 | 65.8 | 4:65 | 65 | 65 | 67 | 194 | MISS |
| fair: derby race mid | 30.6 | 15.3 | 55.7 | 62.8 | 3:5 4:60 | 65 | 60 | 67 | - | MISS |
| fair: derby race hellfire | 30.0 | 15.0 | 59.5 | 65.3 | 4:65 | 65 | 65 | 67 | 184 | MISS |
| fair: memory title | 30.0 | 15.0 | 56.0 | 56.9 | 4:45 | 45 | 45 | 67 | 117 | MISS |
| fair: memory play sweet | 30.0 | 29.5 | 32.0 | 42.9 | 2:124 3:4 | 4 | 0 | 50 | 108 | hitch |
| fair: memory play hellfire | 30.0 | 21.5 | 38.4 | 42.7 | 2:19 3:74 | 74 | 0 | 50 | 117 | MISS |
| fair: wheel intro | 10.7 | 6.4 | 152.1 | 153.2 | 9:36 10:18 | 54 | 54 | 167 | 161 | MISS |
| fair: wheel spin | 10.9 | 6.5 | 151.0 | 174.7 | 9:65 10:12 11:1 | 78 | 78 | 184 | - | MISS |

**fishcook**

| window | logic t/s | drawn fps | work avg ms | work max ms | frames by vblanks | >33 ms | >50 ms | worst frame ms | load ms | verdict |
|---|---:|---:|---:|---:|---|---:|---:|---:|---:|---|
| fish: pier day | 30.0 | 30.0 | 27.9 | 29.8 | 2:130 | 0 | 0 | 34 | 60 | ok |
| fish: bot cast and wait | 29.9 | 29.7 | 26.0 | 45.5 | 2:126 3:3 | 3 | 0 | 50 | 30 | hitch |
| fish: reel | 30.0 | 30.0 | 28.5 | 29.2 | 2:30 | 0 | 0 | 34 | 34 | ok |
| fish: catch card | 31.0 | 25.5 | 32.5 | 78.6 | 2:60 4:13 5:1 | 14 | 14 | 84 | - | MISS |
| fish: fishdex | 30.0 | 15.0 | 56.0 | 57.7 | 4:45 | 45 | 45 | 67 | 37 | MISS |
| fish: cooler | 30.0 | 30.0 | 29.4 | 30.7 | 2:90 | 0 | 0 | 34 | - | ok |
| fish: night reel | 30.0 | 30.0 | 26.5 | 35.7 | 2:100 | 0 | 0 | 34 | 30 | ok |
| cook: recipe book | 30.0 | 20.0 | 48.5 | 49.7 | 3:60 | 60 | 0 | 50 | 62 | MISS |
| cook: chop | 30.0 | 22.2 | 38.5 | 51.7 | 2:28 3:68 | 68 | 0 | 50 | 47 | MISS |
| cook: stir | 30.0 | 20.3 | 42.6 | 53.6 | 2:3 3:84 4:1 | 85 | 1 | 67 | - | MISS |
| cook: fry | 30.0 | 20.5 | 38.7 | 39.7 | 2:7 3:82 | 82 | 0 | 50 | - | MISS |
| cook: result card | 29.8 | 16.9 | 54.0 | 55.4 | 3:23 4:28 | 51 | 28 | 67 | - | MISS |
| cook: bake | 30.0 | 20.8 | 40.7 | 56.9 | 2:11 3:78 4:1 | 79 | 1 | 67 | 47 | MISS |
| cook: simmer | 30.0 | 22.4 | 39.0 | 76.1 | 2:34 3:61 4:1 5:1 | 63 | 2 | 84 | 47 | MISS |
| cook: counter | 30.0 | 19.0 | 45.2 | 79.1 | 2:6 3:41 4:5 5:5 | 51 | 10 | 84 | 1824 | MISS |
| cook: serving | 30.0 | 19.0 | 44.2 | 73.0 | 3:33 4:4 5:1 | 38 | 5 | 84 | - | MISS |

**library**

| window | logic t/s | drawn fps | work avg ms | work max ms | frames by vblanks | >33 ms | >50 ms | worst frame ms | load ms | verdict |
|---|---:|---:|---:|---:|---|---:|---:|---:|---:|---|
| library: lobby | 30.0 | 19.8 | 41.2 | 70.0 | 3:84 4:2 | 86 | 2 | 67 | 1999 | MISS |
| library: lobby shelf move | 30.5 | 19.3 | 39.6 | 48.9 | 3:19 | 19 | 0 | 50 | - | MISS |
| library: bestiary list | 30.0 | 30.0 | 27.4 | 29.2 | 2:90 | 0 | 0 | 34 | 45 | ok |
| library: bestiary jar | 29.8 | 19.6 | 50.0 | 50.9 | 3:56 4:3 | 59 | 3 | 67 | - | MISS |
| library: almanac list | 30.0 | 30.0 | 32.3 | 34.3 | 2:90 | 0 | 0 | 34 | 78 | ok |
| library: almanac list move | 31.0 | 25.9 | 34.6 | 44.4 | 2:19 3:6 | 6 | 0 | 50 | - | MISS |
| library: almanac profile | 30.7 | 15.7 | 57.0 | 64.3 | 3:5 4:41 | 46 | 41 | 67 | - | MISS |
| library: almanac face | 30.5 | 15.6 | 56.9 | 63.1 | 3:7 4:39 | 46 | 39 | 67 | - | MISS |
| library: weapons frames | 29.8 | 24.9 | 35.5 | 37.1 | 2:46 3:29 | 29 | 0 | 50 | 75 | MISS |
| library: weapons plan | 30.0 | 15.0 | 65.9 | 67.2 | 4:45 | 45 | 45 | 67 | - | MISS |
| library: items | 30.0 | 30.0 | 30.5 | 31.9 | 2:90 | 0 | 0 | 34 | 75 | ok |
| library: items card | 29.8 | 19.2 | 50.1 | 51.6 | 3:54 4:4 | 58 | 4 | 67 | - | MISS |
| library: trophies | 30.0 | 30.0 | 30.0 | 31.5 | 2:90 | 0 | 0 | 34 | 104 | ok |
| library: trophy page | 30.0 | 20.0 | 47.3 | 47.7 | 3:60 | 60 | 0 | 50 | - | MISS |
| library: jukebox | 30.0 | 20.0 | 41.0 | 43.0 | 3:60 | 60 | 0 | 50 | 50 | MISS |
| library: jukebox playing | 30.0 | 19.8 | 41.8 | 43.0 | 3:86 | 86 | 0 | 50 | - | MISS |
| library: gallery textures | 30.0 | 30.0 | 31.4 | 35.4 | 2:90 | 0 | 0 | 34 | 37 | ok |
| library: gallery tiled | 30.0 | 20.0 | 49.4 | 49.7 | 3:60 | 60 | 0 | 50 | - | MISS |
| library: gallery sky | 30.0 | 30.0 | 27.7 | 30.4 | 2:90 | 0 | 0 | 34 | - | ok |
| library: field guide contents | 30.0 | 30.0 | 30.7 | 32.1 | 2:90 | 0 | 0 | 34 | 43 | ok |
| library: field guide page | 30.0 | 20.0 | 42.9 | 43.3 | 3:60 | 60 | 0 | 50 | - | MISS |
| library: field guide page turn | 30.8 | 20.0 | 42.7 | 46.9 | 3:13 | 13 | 0 | 50 | - | MISS |
| library: field guide scroll | 30.8 | 20.0 | 41.9 | 42.1 | 3:13 | 13 | 0 | 50 | - | MISS |

**action**

| window | logic t/s | drawn fps | work avg ms | work max ms | frames by vblanks | >33 ms | >50 ms | worst frame ms | load ms | verdict |
|---|---:|---:|---:|---:|---|---:|---:|---:|---:|---|
| action: level select | 30.2 | 18.8 | 49.9 | 56.5 | 3:63 4:18 | 81 | 18 | 67 | 134 | MISS |
| action: level select move | 30.0 | 15.0 | 55.9 | 57.2 | 4:10 | 10 | 10 | 67 | - | MISS |
| action: breach intro card | 18.3 | 10.2 | 94.5 | 94.8 | 6:50 | 50 | 50 | 100 | - | MISS |
| action: breach play (combat, reference) | 19.1 | 11.3 | 77.5 | 81.9 | 5:32 6:21 | 53 | 53 | 100 | - | MISS |
| action: breach pause menu | 15.9 | 8.8 | 103.6 | 112.0 | 6:10 7:40 | 50 | 50 | 117 | - | MISS |
| action: level select after a level | 29.8 | 17.9 | 50.7 | 55.2 | 3:37 4:17 | 54 | 17 | 67 | - | MISS |
| action: arena intro card | 17.0 | 9.4 | 94.8 | 96.1 | 6:32 7:18 | 50 | 50 | 117 | 173 | MISS |
| action: arena fight (combat, reference) | 16.2 | 9.0 | 91.5 | 126.7 | 5:1 6:27 7:17 8:4 10:1 | 50 | 50 | 167 | - | MISS |
| action: arena pause menu | 15.5 | 8.5 | 100.5 | 110.5 | 6:5 7:28 | 33 | 33 | 117 | - | MISS |
| action: arena results | 15.0 | 8.6 | 106.5 | 114.2 | 7:49 8:3 | 52 | 52 | 134 | - | MISS |
| action: boss rush intro card | 19.2 | 11.1 | 84.1 | 85.4 | 5:31 6:21 | 52 | 52 | 100 | 133 | MISS |
| action: boss rush round 1 (combat, reference) | 17.9 | 9.8 | 93.4 | 109.0 | 6:42 7:7 | 49 | 49 | 117 | - | MISS |
| action: boss rush results | 18.4 | 10.0 | 89.3 | 91.0 | 6:49 | 49 | 49 | 100 | - | MISS |
| action: horde intro card | 18.8 | 11.1 | 84.8 | 86.4 | 5:31 6:22 | 53 | 53 | 100 | 156 | MISS |
| action: horde results | 17.1 | 10.3 | 88.2 | 89.7 | 5:9 6:45 | 54 | 54 | 100 | - | MISS |
| action: mayhem intro card | 17.1 | 10.3 | 86.4 | 86.8 | 5:9 6:45 | 54 | 54 | 100 | 180 | MISS |
| action: mayhem chaos (combat, reference) | 11.0 | 6.6 | 128.0 | 178.5 | 6:1 7:4 8:11 9:17 10:14 11:7 | 54 | 54 | 184 | - | MISS |
| action: workshop | 25.3 | 18.9 | 44.7 | 260.3 | 2:20 3:74 15:2 16+:1 | 77 | 3 | 268 | 286 | MISS |
| action: workshop part change | 30.5 | 23.4 | 37.9 | 39.4 | 2:8 3:15 | 15 | 0 | 50 | - | MISS |
| action: workshop armory | 30.0 | 27.0 | 34.2 | 35.1 | 2:61 3:20 | 20 | 0 | 50 | - | MISS |
| action: weld intro card | 30.0 | 22.7 | 37.8 | 39.1 | 2:22 3:46 | 46 | 0 | 50 | 61 | MISS |
| action: welding | 30.1 | 21.8 | 38.4 | 44.3 | 2:21 3:73 | 73 | 0 | 50 | - | MISS |

**dream**

| window | logic t/s | drawn fps | work avg ms | work max ms | frames by vblanks | >33 ms | >50 ms | worst frame ms | load ms | verdict |
|---|---:|---:|---:|---:|---|---:|---:|---:|---:|---|
| dream: bedroom | 30.0 | 30.0 | 24.4 | 30.8 | 2:90 | 0 | 0 | 34 | 1868 | ok |
| dream: thought bubble | 27.9 | 22.3 | 31.8 | 60.7 | 2:34 3:4 4:6 5:3 6:1 | 14 | 10 | 100 | - | MISS |
| dream: vignette 1 cute | 24.7 | 13.3 | 60.7 | 62.8 | 4:27 5:32 | 59 | 59 | 84 | - | MISS |
| dream: vignette 1 twist | 24.3 | 12.9 | 62.1 | 68.0 | 4:18 5:35 | 53 | 53 | 84 | - | MISS |
| dream: vignette 2 | 24.1 | 12.8 | 62.6 | 65.9 | 4:25 5:44 | 69 | 69 | 84 | - | MISS |
| dream: nightmare vignette 1 | 20.9 | 11.7 | 69.3 | 91.1 | 4:11 5:46 6:13 7:3 | 73 | 73 | 117 | 108 | MISS |
| dream: nightmare vignette 2 | 22.6 | 12.3 | 64.1 | 78.6 | 4:17 5:48 6:6 | 71 | 71 | 100 | - | MISS |
| dream: nightmare vignette 3 (cute, then red) | 17.6 | 10.6 | 80.4 | 84.8 | 5:20 6:40 | 60 | 60 | 100 | - | MISS |
| dream: wake-up choice | 17.6 | 10.6 | 80.2 | 85.9 | 5:12 6:24 | 36 | 36 | 100 | - | MISS |

**house**

| window | logic t/s | drawn fps | work avg ms | work max ms | frames by vblanks | >33 ms | >50 ms | worst frame ms | load ms | verdict |
|---|---:|---:|---:|---:|---|---:|---:|---:|---:|---|
| house: build mode | 30.0 | 30.0 | 12.3 | 16.2 | 2:130 | 0 | 0 | 34 | 192 | ok |
| house: build cursor move | 30.0 | 30.0 | 11.1 | 13.0 | 2:30 | 0 | 0 | 34 | - | ok |
| house: catalog modal | 30.0 | 15.0 | 57.7 | 58.2 | 4:45 | 45 | 45 | 67 | - | MISS |
| house: catalog cursor move | 30.0 | 15.0 | 57.9 | 58.2 | 4:15 | 15 | 15 | 67 | - | MISS |
| house: held piece | 30.3 | 30.3 | 13.4 | 17.2 | 2:60 | 0 | 0 | 34 | - | ok |
| house: rotate view | 29.5 | 27.5 | 14.4 | 106.1 | 2:27 7:1 | 1 | 1 | 117 | - | MISS |
| house: photo mode | 29.8 | 29.8 | 16.1 | 19.8 | 2:90 | 0 | 0 | 34 | - | ok |
| house: photo snap | 30.0 | 30.0 | 11.3 | 14.8 | 2:30 | 0 | 0 | 34 | - | ok |
| house: live mode | 30.0 | 30.0 | 10.2 | 11.5 | 2:130 | 0 | 0 | 34 | - | ok |
| house: score panel | 30.0 | 20.0 | 45.0 | 45.3 | 3:60 | 60 | 0 | 50 | - | MISS |
| house: live, 4 chibis (room-test 1) | 30.0 | 26.8 | 32.4 | 47.7 | 2:89 3:27 | 27 | 0 | 50 | - | MISS |
| house: defend tutorial | 30.0 | 30.0 | 28.7 | 35.6 | 2:130 | 0 | 0 | 34 | 132 | ok |
| house: defend siege wave 3 | 30.0 | 29.5 | 31.3 | 37.2 | 2:124 3:4 | 4 | 0 | 50 | 195 | hitch |
| house: defend siege freeze + double clock | 30.0 | 30.0 | 30.4 | 35.4 | 2:90 | 0 | 0 | 34 | - | ok |
| house: visit (Nana) | 30.0 | 30.0 | 12.8 | 15.9 | 2:130 | 0 | 0 | 34 | 1714 | ok |
| house: homemart | 30.0 | 30.0 | 29.1 | 29.9 | 2:90 | 0 | 0 | 34 | 52 | ok |
| house: homemart cursor move | 30.0 | 30.0 | 31.6 | 31.9 | 2:30 | 0 | 0 | 34 | - | ok |
| house: homemart season tab | 30.0 | 30.0 | 26.9 | 28.3 | 2:60 | 0 | 0 | 34 | - | ok |
| house: black market | 30.0 | 29.7 | 33.1 | 34.6 | 2:87 3:2 | 2 | 0 | 50 | 52 | hitch |

**studio**

| window | logic t/s | drawn fps | work avg ms | work max ms | frames by vblanks | >33 ms | >50 ms | worst frame ms | load ms | verdict |
|---|---:|---:|---:|---:|---|---:|---:|---:|---:|---|
| studio: choir quick fill | 30.0 | 15.0 | 58.0 | 60.3 | 4:30 | 30 | 30 | 67 | 1768 | MISS |
| studio: choir member card | 30.0 | 15.0 | 54.2 | 56.3 | 4:45 | 45 | 45 | 67 | - | MISS |
| studio: citizen picker | 28.1 | 14.4 | 61.5 | 64.6 | 4:38 5:8 | 46 | 46 | 84 | - | MISS |
| studio: citizen picker move | 23.7 | 12.6 | 63.7 | 77.6 | 4:10 5:2 6:3 8:1 | 16 | 16 | 134 | - | MISS |
| studio: stage tab | 30.0 | 22.8 | 36.1 | 38.1 | 2:37 3:62 | 62 | 0 | 50 | - | MISS |
| studio: file tab | 30.0 | 30.0 | 21.4 | 22.8 | 2:90 | 0 | 0 | 34 | - | ok |
| studio: song book | 30.0 | 20.0 | 44.5 | 47.6 | 3:60 | 60 | 0 | 50 | - | MISS |
| studio: templates | 30.2 | 29.8 | 29.0 | 35.9 | 2:88 3:1 | 1 | 0 | 50 | - | hitch |
| studio: templates move | 30.0 | 30.0 | 28.7 | 29.0 | 2:30 | 0 | 0 | 34 | - | ok |

**creator**

| window | logic t/s | drawn fps | work avg ms | work max ms | frames by vblanks | >33 ms | >50 ms | worst frame ms | load ms | verdict |
|---|---:|---:|---:|---:|---|---:|---:|---:|---:|---|
| creator: species and body tab | 30.0 | 30.0 | 15.4 | 34.1 | 2:90 | 0 | 0 | 34 | 196 | ok |
| creator: tab change to head and face | 30.0 | 28.5 | 21.9 | 59.0 | 2:17 3:1 4:1 | 2 | 1 | 67 | - | MISS |
| creator: head and face tab | 30.0 | 29.7 | 16.4 | 60.3 | 2:88 4:1 | 1 | 1 | 67 | - | hitch |
| creator: tab change to hair | 30.8 | 29.2 | 19.7 | 55.2 | 2:17 3:1 4:1 | 2 | 1 | 67 | - | MISS |
| creator: hair tab | 30.2 | 29.8 | 15.6 | 50.9 | 2:88 3:1 | 1 | 0 | 50 | - | hitch |
| creator: tab change to supernatural | 32.4 | 29.2 | 18.8 | 56.4 | 2:16 3:1 4:1 | 2 | 1 | 67 | - | MISS |
| creator: supernatural tab | 30.0 | 29.7 | 15.8 | 55.8 | 2:88 4:1 | 1 | 1 | 67 | - | hitch |
| creator: tab change to outfit | 32.4 | 29.2 | 25.9 | 67.3 | 2:16 3:1 4:1 | 2 | 1 | 67 | - | MISS |
| creator: outfit tab | 30.0 | 30.0 | 18.3 | 36.2 | 2:90 | 0 | 0 | 34 | - | ok |
| creator: tab change to voice | 32.4 | 29.2 | 24.2 | 55.5 | 2:16 3:1 4:1 | 2 | 1 | 67 | - | MISS |
| creator: voice tab | 30.0 | 30.0 | 15.4 | 33.5 | 2:90 | 0 | 0 | 34 | - | ok |
| creator: tab change to personality | 31.6 | 30.0 | 26.3 | 44.1 | 2:18 3:1 | 1 | 0 | 50 | - | hitch |
| creator: personality tab | 30.0 | 30.0 | 24.4 | 26.2 | 2:90 | 0 | 0 | 34 | - | ok |
| creator: tab change to profile | 31.6 | 30.0 | 19.3 | 50.9 | 2:17 3:2 | 2 | 0 | 50 | - | hitch |
| creator: profile tab | 30.0 | 30.0 | 15.4 | 28.6 | 2:90 | 0 | 0 | 34 | - | ok |
| creator: tab change to home | 31.6 | 30.0 | 27.4 | 44.6 | 2:18 3:1 | 1 | 0 | 50 | - | hitch |
| creator: home tab | 30.2 | 29.8 | 25.4 | 43.4 | 2:87 3:2 | 2 | 0 | 50 | - | hitch |
| creator: grid (first row) | 30.0 | 30.0 | 15.3 | 16.6 | 2:90 | 0 | 0 | 34 | - | ok |
| creator: grid cursor move | 30.0 | 30.0 | 24.5 | 37.2 | 2:30 | 0 | 0 | 34 | - | ok |
| creator: hat grid (bust camera) | 29.8 | 27.2 | 21.6 | 66.5 | 2:68 3:13 4:1 | 14 | 1 | 67 | - | MISS |
| creator: hat grid try-on move | 28.6 | 15.2 | 51.5 | 77.0 | 2:1 3:6 4:6 5:2 6:1 | 15 | 9 | 100 | - | MISS |
| creator: hat grid filter chip | 31.0 | 21.7 | 38.9 | 49.1 | 2:2 3:19 | 19 | 0 | 50 | - | MISS |
| creator: colour swatches | 30.0 | 30.0 | 31.9 | 33.6 | 2:90 | 0 | 0 | 34 | - | ok |
| creator: swatch move | 29.0 | 25.2 | 36.3 | 99.6 | 2:20 3:5 6:1 | 6 | 1 | 100 | - | MISS |
| creator: start menu | 22.9 | 12.2 | 69.9 | 81.0 | 4:11 5:35 6:2 | 48 | 48 | 100 | - | MISS |

**life**

| window | logic t/s | drawn fps | work avg ms | work max ms | frames by vblanks | >33 ms | >50 ms | worst frame ms | load ms | verdict |
|---|---:|---:|---:|---:|---|---:|---:|---:|---:|---|
| life: new island -> hub (the load) | 13.2 | 8.4 | 101.9 | 1998.3 | 2:9 4:9 5:5 6:22 7:1 16+:1 | 38 | 38 | 2009 | 38 | MISS |
| life: family dashboard | 30.0 | 30.0 | 22.4 | 24.0 | 2:90 | 0 | 0 | 34 | - | ok |
| life: add member: kind | 30.0 | 25.5 | 35.1 | 45.8 | 2:34 3:17 | 17 | 0 | 50 | - | MISS |
| life: add member: pick a resident | 30.0 | 30.0 | 30.3 | 32.2 | 2:60 | 0 | 0 | 34 | - | ok |
| life: add member: pick move | 30.5 | 25.4 | 34.9 | 95.1 | 2:21 3:1 4:2 6:1 | 4 | 3 | 100 | - | MISS |
| life: add member: role | 30.0 | 27.0 | 34.4 | 45.1 | 2:42 3:12 | 12 | 0 | 50 | - | MISS |
| life: family with a member | 30.0 | 30.0 | 26.3 | 30.3 | 2:90 | 0 | 0 | 34 | - | ok |
| life: play-as switcher | 30.0 | 30.0 | 15.3 | 16.0 | 2:90 | 0 | 0 | 34 | 147 | ok |
| life: play-as move | 30.0 | 30.0 | 15.3 | 15.8 | 2:30 | 0 | 0 | 34 | - | ok |
| life: options controls | 30.0 | 30.0 | 26.0 | 28.2 | 2:60 | 0 | 0 | 34 | 29 | ok |
| life: options page turn to shooter | 30.0 | 30.0 | 25.6 | 25.9 | 2:20 | 0 | 0 | 34 | - | ok |
| life: options shooter | 30.0 | 30.0 | 25.6 | 29.0 | 2:60 | 0 | 0 | 34 | - | ok |
| life: options page turn to life | 30.0 | 30.0 | 25.7 | 27.7 | 2:20 | 0 | 0 | 34 | - | ok |
| life: options life | 30.0 | 30.0 | 25.8 | 26.4 | 2:60 | 0 | 0 | 34 | - | ok |
| life: options page turn to audio | 30.0 | 30.0 | 24.9 | 25.5 | 2:20 | 0 | 0 | 34 | - | ok |
| life: options audio | 30.0 | 30.0 | 25.1 | 27.6 | 2:60 | 0 | 0 | 34 | - | ok |
| life: options page turn to display | 30.0 | 30.0 | 23.9 | 24.3 | 2:20 | 0 | 0 | 34 | - | ok |
| life: options display | 30.0 | 30.0 | 24.2 | 27.3 | 2:60 | 0 | 0 | 34 | - | ok |
| life: options page turn to performance | 30.0 | 30.0 | 24.1 | 24.4 | 2:20 | 0 | 0 | 34 | - | ok |
| life: options performance | 30.0 | 30.0 | 24.3 | 27.8 | 2:60 | 0 | 0 | 34 | - | ok |
| life: options page turn to accessibility | 27.9 | 23.7 | 32.1 | 47.4 | 2:12 3:3 4:2 | 5 | 2 | 67 | - | MISS |
| life: options accessibility | 30.3 | 28.2 | 27.9 | 42.5 | 2:47 3:9 | 9 | 0 | 50 | - | MISS |
| life: options page turn to cheats | 30.0 | 30.0 | 24.4 | 24.9 | 2:20 | 0 | 0 | 34 | - | ok |
| life: options cheats | 30.0 | 30.0 | 24.5 | 27.2 | 2:60 | 0 | 0 | 34 | - | ok |
| life: options page turn to game | 30.0 | 30.0 | 23.8 | 24.3 | 2:20 | 0 | 0 | 34 | - | ok |
| life: options game | 30.0 | 30.0 | 24.1 | 27.1 | 2:60 | 0 | 0 | 34 | - | ok |
| life: options cursor move | 30.0 | 30.0 | 24.1 | 26.5 | 2:30 | 0 | 0 | 34 | - | ok |
| life: save/load | 30.0 | 30.0 | 23.1 | 25.7 | 2:90 | 0 | 0 | 34 | 39 | ok |
| life: save/load cursor move | 30.0 | 30.0 | 23.3 | 26.0 | 2:30 | 0 | 0 | 34 | - | ok |
| life: saving (the save freeze) | 3.4 | 3.4 | 292.1 | 11424.8 | 2:42 3:1 16+:1 | 2 | 1 | 11435 | - | MISS |
| life: care popup (feed) | 29.0 | 14.5 | 58.3 | 67.1 | 4:26 5:4 | 30 | 30 | 84 | 120 | MISS |
| life: care reaction | 30.5 | 17.3 | 47.9 | 59.8 | 3:18 4:15 5:1 | 34 | 16 | 84 | - | MISS |
| life: shop (food) | 30.2 | 29.8 | 32.1 | 43.0 | 2:88 3:1 | 1 | 0 | 50 | 115 | hitch |
| life: shop cursor move | 29.5 | 29.5 | 32.9 | 37.7 | 2:29 3:1 | 1 | 0 | 50 | - | hitch |
| life: shop quantity popup | 25.0 | 13.3 | 59.7 | 64.1 | 4:15 5:17 | 32 | 32 | 84 | - | MISS |
| life: shop gifts tab | 30.0 | 30.0 | 29.5 | 37.5 | 2:60 | 0 | 0 | 34 | - | ok |
| life: black market | 29.8 | 22.9 | 36.3 | 60.4 | 2:49 3:5 4:9 5:5 6:1 | 20 | 15 | 100 | 132 | MISS |
| life: cafe counter | 29.8 | 29.8 | 30.5 | 38.6 | 2:89 3:1 | 1 | 0 | 50 | 119 | hitch |
| life: news help wanted | 30.0 | 30.0 | 29.6 | 33.8 | 2:90 | 0 | 0 | 34 | 60 | ok |
| life: news third tab | 30.0 | 30.0 | 32.4 | 35.0 | 2:90 | 0 | 0 | 34 | - | ok |
| life: graveyard | 30.0 | 30.0 | 13.8 | 15.9 | 2:90 | 0 | 0 | 34 | 17 | ok |
| life: unlocks | 19.1 | 11.2 | 66.9 | 71.7 | 4:14 5:5 6:34 | 53 | 53 | 100 | 90 | MISS |
| life: unlocks cursor move | 20.0 | 11.3 | 65.9 | 69.9 | 4:4 5:4 6:9 | 17 | 17 | 100 | - | MISS |
| life: unlocks paints tab | 30.0 | 15.0 | 40.0 | 40.5 | 4:30 | 30 | 30 | 67 | - | MISS |
| life: citizen picker | 30.0 | 29.7 | 31.5 | 44.3 | 2:87 3:2 | 2 | 0 | 50 | 74 | hitch |
| life: citizen picker move | 30.5 | 27.5 | 33.2 | 96.7 | 2:25 3:1 6:1 | 2 | 1 | 100 | - | MISS |

**story**

| window | logic t/s | drawn fps | work avg ms | work max ms | frames by vblanks | >33 ms | >50 ms | worst frame ms | load ms | verdict |
|---|---:|---:|---:|---:|---|---:|---:|---:|---:|---|
| story: first boot language picker | 30.0 | 30.0 | 12.6 | 14.1 | 2:90 | 0 | 0 | 34 | 15 | ok |
| story: language picker move | 30.0 | 30.0 | 12.4 | 13.3 | 2:30 | 0 | 0 | 34 | - | ok |
| story: first boot security code | 30.0 | 30.0 | 14.2 | 16.3 | 2:90 | 0 | 0 | 34 | 14 | ok |
| story: security code digit | 30.0 | 30.0 | 14.1 | 14.9 | 2:30 | 0 | 0 | 34 | - | ok |
| story: title -> new story (the load) | 12.5 | 11.9 | 81.5 | 2068.6 | 2:36 3:4 4:1 16+:1 | 6 | 2 | 2076 | 174 | MISS |
| story: prologue cutscene | 30.2 | 30.2 | 32.1 | 33.4 | 2:90 | 0 | 0 | 34 | - | ok |
| story: quest log story tab | 30.0 | 30.0 | 21.8 | 22.6 | 2:90 | 0 | 0 | 34 | 30 | ok |
| story: quest log side tab | 30.0 | 30.0 | 29.8 | 32.4 | 2:90 | 0 | 0 | 34 | 94 | ok |
| story: quest log side cursor move | 29.5 | 29.5 | 29.9 | 33.3 | 2:29 3:1 | 1 | 0 | 50 | - | hitch |
| story: quest log items tab | 30.0 | 30.0 | 20.6 | 21.8 | 2:90 | 0 | 0 | 34 | 24 | ok |
| story: quest log map tab | 30.0 | 30.0 | 27.5 | 28.2 | 2:90 | 0 | 0 | 34 | 83 | ok |
| story: quest log map cursor move | 30.0 | 30.0 | 27.4 | 27.9 | 2:30 | 0 | 0 | 34 | - | ok |
| story: quest log friends tab | 30.0 | 30.0 | 24.7 | 25.6 | 2:90 | 0 | 0 | 34 | 228 | ok |
| story: quest log trophies tab | 30.0 | 30.0 | 18.9 | 20.4 | 2:90 | 0 | 0 | 34 | 23 | ok |
| story: cutscene chapter card 2 | 30.0 | 15.0 | 58.9 | 60.8 | 4:45 | 45 | 45 | 67 | 84 | MISS |
| story: cutscene chapter card 3 | 29.8 | 29.8 | 17.3 | 30.6 | 2:90 | 0 | 0 | 34 | 69 | ok |
| story: cutscene chapter card 4 | 30.2 | 23.5 | 36.1 | 85.5 | 2:55 3:3 5:12 | 15 | 12 | 84 | 55 | MISS |
| story: cutscene home vignette | 30.0 | 20.0 | 41.4 | 46.0 | 3:60 | 60 | 0 | 50 | 100 | MISS |
| story: cutscene fog vignette | 18.4 | 10.0 | 93.3 | 102.1 | 6:49 | 49 | 49 | 100 | 144 | MISS |
| story: cutscene eruption vignette | 30.0 | 16.7 | 55.2 | 62.9 | 3:18 4:32 | 50 | 32 | 67 | 34 | MISS |
| story: cutscene vapula vignette | 21.7 | 12.0 | 75.9 | 81.0 | 4:1 5:49 | 50 | 50 | 84 | 88 | MISS |
| story: cutscene after a press | 30.8 | 17.9 | 51.3 | 65.8 | 3:25 4:10 | 35 | 10 | 67 | - | MISS |
| story: credits | 7.2 | 7.2 | 135.7 | 470.7 | 2:92 3:4 4:1 16+:33 | 38 | 34 | 469 | 152 | MISS |
| story: credits parade | 5.3 | 5.2 | 193.3 | 672.8 | 2:58 3:29 4:4 5:5 16+:33 | 71 | 42 | 686 | - | MISS |


## 2. Results: PS1 (every window)

Tags are the GBA window names in lower case. PS1 logic = drawn fps. The `fair` group stops at the Dodge title
card (the PS1 hang, S2-12); its remaining windows were played by a second script from the Dodge section on
(rows after the gap).

**fair**

| window | fps (= logic) | frames by vsyncs | >33 ms | >50 ms | worst ms | overlay load ms | verdict |
|---|---:|---|---:|---:|---:|---:|---|
| fair_catch_title | 30.17 | 2:89 | 0 | 0 | 33 | 288+260 | ok |
| fair_catch_play_sweet | 30.12 | 2:129 | 0 | 0 | 33 | 287+260 | ok |
| fair_catch_results | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| fair_catch_play_hellfire | 30.12 | 2:129 | 0 | 0 | 33 | 287+260 | ok |
| fair_whack_title | 30.17 | 2:89 | 0 | 0 | 33 | 287+260 | ok |
| fair_whack_play_sweet | 30.12 | 2:129 | 0 | 0 | 33 | 287+260 | ok |
| fair_whack_play_hellfire | 30.12 | 2:129 | 0 | 0 | 33 | 287+260 | ok |
| fair_dodge_title | 30.17 | 2:89 | 0 | 0 | 33 | 288+260 | ok |
| fair_dodge_play_sweet | 30.12 | 2:129 | 0 | 0 | 33 | 287+260 | ok |
| fair_dodge_play_late | 30.12 | 2:129 | 0 | 0 | 33 | - | ok |
| fair_dodge_play_hellfire | 30.12 | 2:129 | 0 | 0 | 33 | 287+260 | ok |
| fair_gallery_title | 30.17 | 2:89 | 0 | 0 | 33 | 287+260 | ok |
| fair_gallery_play_sweet | 30.00 | 2:128 3:1 | 0 | 0 | 50 | 287+260 | ok |
| fair_gallery_play_hellfire | 30.00 | 2:128 3:1 | 0 | 0 | 50 | 287+260 | ok |
| fair_derby_title | 30.17 | 2:89 | 0 | 0 | 33 | 287+260 | ok |
| fair_derby_race_sweet | 30.00 | 2:128 3:1 | 0 | 0 | 50 | 287+260 | ok |
| fair_derby_race_mid | 30.00 | 2:128 3:1 | 0 | 0 | 50 | - | ok |
| fair_derby_race_hellfire | 30.00 | 2:128 3:1 | 0 | 0 | 50 | 287+260 | ok |
| fair_memory_title | 30.17 | 2:89 | 0 | 0 | 33 | 287+260 | ok |
| fair_memory_play_sweet | 30.12 | 2:129 | 0 | 0 | 33 | 287+260 | ok |
| fair_memory_play_hellfire | 30.12 | 2:129 | 0 | 0 | 33 | 287+260 | ok |
| fair_wheel_intro | 22.41 | 2:29 3:59 4:1 | 60 | 1 | 67 | 287+260 | MISS |
| fair_wheel_spin | 22.41 | 2:44 3:83 4:1 5:1 | 85 | 2 | 83 | - | MISS |

**fishcook**

| window | fps (= logic) | frames by vsyncs | >33 ms | >50 ms | worst ms | overlay load ms | verdict |
|---|---:|---|---:|---:|---:|---:|---|
| fish_pier_day | 30.12 | 2:129 | 0 | 0 | 33 | 288+267 | ok |
| fish_bot_cast_and_wait | 30.12 | 2:129 | 0 | 0 | 33 | - | ok |
| fish_reel | 30.51 | 2:29 | 0 | 0 | 33 | - | ok |
| fish_catch_card | 30.00 | 2:88 3:1 | 0 | 0 | 50 | - | ok |
| fish_fishdex | 30.00 | 2:88 3:1 | 0 | 0 | 50 | - | ok |
| fish_cooler | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| fish_night_reel | 30.15 | 2:99 | 0 | 0 | 33 | - | ok |
| cook_recipe_book | 30.17 | 2:89 | 0 | 0 | 33 | 287+267 | ok |
| cook_chop | 30.12 | 2:129 | 0 | 0 | 33 | - | ok |
| cook_stir | 30.12 | 2:129 | 0 | 0 | 33 | - | ok |
| cook_fry | 30.12 | 2:129 | 0 | 0 | 33 | - | ok |
| cook_result_card | 30.00 | 2:88 3:1 | 0 | 0 | 50 | - | ok |
| cook_bake | 30.12 | 2:129 | 0 | 0 | 33 | - | ok |
| cook_simmer | 30.12 | 2:129 | 0 | 0 | 33 | - | ok |
| cook_counter | 29.83 | 2:87 3:2 | 2 | 0 | 50 | - | hitch |
| cook_serving | 30.00 | 2:58 3:1 | 0 | 0 | 50 | - | ok |

**library**

| window | fps (= logic) | frames by vsyncs | >33 ms | >50 ms | worst ms | overlay load ms | verdict |
|---|---:|---|---:|---:|---:|---:|---|
| library_lobby | 30.12 | 2:129 | 0 | 0 | 33 | 267 | ok |
| library_lobby_shelf_move | 29.51 | 2:28 4:1 | 1 | 1 | 67 | - | hitch |
| library_bestiary_list | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| library_bestiary_jar | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| library_almanac_list | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| library_almanac_list_move | 29.51 | 2:28 4:1 | 1 | 1 | 67 | - | hitch |
| library_almanac_profile | 30.00 | 2:88 3:1 | 0 | 0 | 50 | - | ok |
| library_almanac_face | 30.00 | 2:88 3:1 | 0 | 0 | 50 | - | ok |
| library_weapons_frames | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| library_weapons_plan | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| library_items | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| library_items_card | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| library_trophies | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| library_trophy_page | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| library_jukebox | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| library_jukebox_playing | 30.00 | 2:128 3:1 | 0 | 0 | 50 | - | ok |
| library_gallery_textures | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| library_gallery_tiled | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| library_gallery_sky | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| library_field_guide_contents | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| library_field_guide_page | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| library_field_guide_page_turn | 30.77 | 2:19 | 0 | 0 | 33 | - | ok |
| library_field_guide_scroll | 30.77 | 2:19 | 0 | 0 | 33 | - | ok |

**action**

| window | fps (= logic) | frames by vsyncs | >33 ms | >50 ms | worst ms | overlay load ms | verdict |
|---|---:|---|---:|---:|---:|---:|---|
| action_level_select | 30.00 | 2:128 3:1 | 0 | 0 | 50 | 288+341 | ok |
| action_level_select_move | 30.77 | 2:19 | 0 | 0 | 33 | - | ok |
| action_breach_intro_card | 22.88 | 2:34 3:54 4:1 | 55 | 1 | 67 | - | MISS |
| action_breach_play_combat_reference | 30.00 | 2:88 3:1 | 0 | 0 | 50 | - | ok |
| action_breach_pause_menu | 30.00 | 2:88 3:1 | 0 | 0 | 50 | - | ok |
| action_level_select_after_a_level | 30.00 | 2:88 3:1 | 0 | 0 | 50 | - | ok |
| action_arena_intro_card | 30.00 | 2:88 3:1 | 0 | 0 | 50 | 287+341 | ok |
| action_arena_fight_combat_reference | 30.00 | 2:88 3:1 | 0 | 0 | 50 | - | ok |
| action_arena_pause_menu | 30.00 | 2:58 3:1 | 0 | 0 | 50 | - | ok |
| action_arena_results | 30.00 | 2:88 3:1 | 0 | 0 | 50 | - | ok |
| action_boss_rush_intro_card | 30.00 | 2:88 3:1 | 0 | 0 | 50 | 287+341 | ok |
| action_boss_rush_round_1_combat_reference | 30.00 | 2:88 3:1 | 0 | 0 | 50 | - | ok |
| action_boss_rush_results | 30.00 | 2:88 3:1 | 0 | 0 | 50 | - | ok |
| action_horde_intro_card | 30.00 | 2:88 3:1 | 0 | 0 | 50 | 287+341 | ok |
| action_horde_results | 30.00 | 2:88 3:1 | 0 | 0 | 50 | - | ok |
| action_mayhem_intro_card | 30.00 | 2:88 3:1 | 0 | 0 | 50 | 287+341 | ok |
| action_mayhem_chaos_combat_reference | 29.51 | 2:85 3:4 | 4 | 0 | 50 | - | hitch |
| action_workshop | 28.78 | 2:126 6:3 | 3 | 3 | 100 | 287+107 | MISS |
| action_workshop_part_change | 30.51 | 2:29 | 0 | 0 | 33 | - | ok |
| action_workshop_armory | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| action_weld_intro_card | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| action_welding | 30.00 | 2:128 3:1 | 0 | 0 | 50 | - | ok |

**dream**

| window | fps (= logic) | frames by vsyncs | >33 ms | >50 ms | worst ms | overlay load ms | verdict |
|---|---:|---|---:|---:|---:|---:|---|
| dream_bedroom | 30.17 | 2:89 | 0 | 0 | 33 | 161 | ok |
| dream_thought_bubble | 30.25 | 2:59 | 0 | 0 | 33 | - | ok |
| dream_vignette_1_cute | 30.00 | 2:108 3:1 | 0 | 0 | 50 | - | ok |
| dream_vignette_1_twist | 30.00 | 2:98 3:1 | 0 | 0 | 50 | - | ok |
| dream_vignette_2 | 30.00 | 2:128 3:1 | 0 | 0 | 50 | - | ok |
| dream_nightmare_vignette_1 | 30.00 | 2:128 3:1 | 0 | 0 | 50 | - | ok |
| dream_nightmare_vignette_2 | 29.89 | 2:128 4:1 | 1 | 1 | 67 | - | hitch |
| dream_nightmare_vignette_3_cute_then_red | 29.85 | 2:98 4:1 | 1 | 1 | 67 | - | hitch |
| dream_wake_up_choice | 29.75 | 2:58 4:1 | 1 | 1 | 67 | - | hitch |

**house**

| window | fps (= logic) | frames by vsyncs | >33 ms | >50 ms | worst ms | overlay load ms | verdict |
|---|---:|---|---:|---:|---:|---:|---|
| house_build_mode | 30.12 | 2:129 | 0 | 0 | 33 | 482 | ok |
| house_build_cursor_move | 30.51 | 2:29 | 0 | 0 | 33 | - | ok |
| house_catalog_modal | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| house_catalog_cursor_move | 30.51 | 2:29 | 0 | 0 | 33 | - | ok |
| house_held_piece | 30.25 | 2:59 | 0 | 0 | 33 | - | ok |
| house_rotate_view | 30.51 | 2:29 | 0 | 0 | 33 | - | ok |
| house_photo_mode | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| house_photo_snap | 30.51 | 2:29 | 0 | 0 | 33 | - | ok |
| house_live_mode | 30.12 | 2:129 | 0 | 0 | 33 | - | ok |
| house_score_panel | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| house_live_4_chibis_room_test_1 | 28.89 | 2:120 3:8 4:1 | 9 | 1 | 67 | 80 | MISS |
| house_defend_tutorial | 30.12 | 2:129 | 0 | 0 | 33 | 288+481+80 | ok |
| house_defend_siege_wave_3 | 30.12 | 2:129 | 0 | 0 | 33 | 288+481+80 | ok |
| house_defend_siege_freeze_double_clock | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| house_visit_nana | 30.12 | 2:129 | 0 | 0 | 33 | 288+481+87 | ok |
| house_homemart | 30.17 | 2:89 | 0 | 0 | 33 | 288+481+93 | ok |
| house_homemart_cursor_move | 30.51 | 2:29 | 0 | 0 | 33 | - | ok |
| house_homemart_season_tab | 30.25 | 2:59 | 0 | 0 | 33 | - | ok |
| house_black_market | 30.17 | 2:89 | 0 | 0 | 33 | 288+481+93 | ok |

**studio**

| window | fps (= logic) | frames by vsyncs | >33 ms | >50 ms | worst ms | overlay load ms | verdict |
|---|---:|---|---:|---:|---:|---:|---|
| studio_choir_quick_fill | 30.25 | 2:59 | 0 | 0 | 33 | 288+254 | ok |
| studio_choir_member_card | 30.00 | 2:88 3:1 | 0 | 0 | 50 | - | ok |
| studio_citizen_picker | 30.00 | 2:88 3:1 | 0 | 0 | 50 | - | ok |
| studio_citizen_picker_move | 30.00 | 2:28 3:1 | 0 | 0 | 50 | - | ok |
| studio_stage_tab | 30.12 | 2:129 | 0 | 0 | 33 | - | ok |
| studio_file_tab | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| studio_song_book | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| studio_templates | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| studio_templates_move | 30.51 | 2:29 | 0 | 0 | 33 | - | ok |

**creator**

| window | fps (= logic) | frames by vsyncs | >33 ms | >50 ms | worst ms | overlay load ms | verdict |
|---|---:|---|---:|---:|---:|---:|---|
| creator_species_and_body_tab | 30.17 | 2:89 | 0 | 0 | 33 | 288+394 | ok |
| creator_tab_change_to_head_and_face | 30.77 | 2:19 | 0 | 0 | 33 | - | ok |
| creator_head_and_face_tab | 30.00 | 2:88 3:1 | 0 | 0 | 50 | - | ok |
| creator_tab_change_to_hair | 30.00 | 2:18 3:1 | 0 | 0 | 50 | - | ok |
| creator_hair_tab | 30.00 | 2:88 3:1 | 0 | 0 | 50 | - | ok |
| creator_tab_change_to_supernatural | 30.00 | 2:18 3:1 | 0 | 0 | 50 | - | ok |
| creator_supernatural_tab | 30.00 | 2:88 3:1 | 0 | 0 | 50 | - | ok |
| creator_tab_change_to_outfit | 30.00 | 2:18 3:1 | 0 | 0 | 50 | - | ok |
| creator_outfit_tab | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| creator_tab_change_to_voice | 30.77 | 2:19 | 0 | 0 | 33 | - | ok |
| creator_voice_tab | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| creator_tab_change_to_personality | 30.00 | 2:18 3:1 | 0 | 0 | 50 | - | ok |
| creator_personality_tab | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| creator_tab_change_to_profile | 30.77 | 2:19 | 0 | 0 | 33 | - | ok |
| creator_profile_tab | 30.00 | 2:88 3:1 | 0 | 0 | 50 | - | ok |
| creator_tab_change_to_home | 30.77 | 2:19 | 0 | 0 | 33 | - | ok |
| creator_home_tab | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| creator_grid_first_row | 30.00 | 2:88 3:1 | 0 | 0 | 50 | - | ok |
| creator_grid_cursor_move | 30.00 | 2:28 3:1 | 0 | 0 | 50 | - | ok |
| creator_hat_grid_bust_camera | 30.00 | 2:88 3:1 | 0 | 0 | 50 | - | ok |
| creator_hat_grid_try_on_move | 28.57 | 2:26 3:2 4:1 | 3 | 1 | 67 | - | MISS |
| creator_hat_grid_filter_chip | 30.00 | 2:28 3:1 | 0 | 0 | 50 | - | ok |
| creator_colour_swatches | 30.00 | 2:88 3:1 | 0 | 0 | 50 | - | ok |
| creator_swatch_move | 30.00 | 2:28 3:1 | 0 | 0 | 50 | - | ok |
| creator_start_menu | 30.00 | 2:88 3:1 | 0 | 0 | 50 | - | ok |

**life**

| window | fps (= logic) | frames by vsyncs | >33 ms | >50 ms | worst ms | overlay load ms | verdict |
|---|---:|---|---:|---:|---:|---:|---|
| life_new_island_hub_the_load | 14.52 | 2:25 3:40 4:1 106:1 | 42 | 2 | 1767 | 288+180+287+180 | MISS |
| life_family_dashboard | 30.17 | 2:89 | 0 | 0 | 33 | 287+180+287 | ok |
| life_add_member_kind | 30.25 | 2:59 | 0 | 0 | 33 | - | ok |
| life_add_member_pick_a_resident | 30.00 | 2:58 3:1 | 0 | 0 | 50 | - | ok |
| life_add_member_pick_move | 30.51 | 2:29 | 0 | 0 | 33 | - | ok |
| life_add_member_role | 30.25 | 2:59 | 0 | 0 | 33 | - | ok |
| life_family_with_a_member | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| life_play_as_switcher | 30.17 | 2:89 | 0 | 0 | 33 | 180+287 | ok |
| life_play_as_move | 30.51 | 2:29 | 0 | 0 | 33 | - | ok |
| life_options_controls | 30.25 | 2:59 | 0 | 0 | 33 | 180+287 | ok |
| life_options_page_turn_to_shooter | 30.77 | 2:19 | 0 | 0 | 33 | - | ok |
| life_options_shooter | 30.25 | 2:59 | 0 | 0 | 33 | - | ok |
| life_options_page_turn_to_life | 30.77 | 2:19 | 0 | 0 | 33 | - | ok |
| life_options_life | 30.25 | 2:59 | 0 | 0 | 33 | - | ok |
| life_options_page_turn_to_audio | 30.77 | 2:19 | 0 | 0 | 33 | - | ok |
| life_options_audio | 30.25 | 2:59 | 0 | 0 | 33 | - | ok |
| life_options_page_turn_to_display | 30.77 | 2:19 | 0 | 0 | 33 | - | ok |
| life_options_display | 30.25 | 2:59 | 0 | 0 | 33 | - | ok |
| life_options_page_turn_to_performance | 30.77 | 2:19 | 0 | 0 | 33 | - | ok |
| life_options_performance | 30.25 | 2:59 | 0 | 0 | 33 | - | ok |
| life_options_page_turn_to_accessibility | 30.77 | 2:19 | 0 | 0 | 33 | - | ok |
| life_options_accessibility | 30.25 | 2:59 | 0 | 0 | 33 | - | ok |
| life_options_page_turn_to_cheats | 30.77 | 2:19 | 0 | 0 | 33 | - | ok |
| life_options_cheats | 30.25 | 2:59 | 0 | 0 | 33 | - | ok |
| life_options_page_turn_to_game | 30.77 | 2:19 | 0 | 0 | 33 | - | ok |
| life_options_game | 30.25 | 2:59 | 0 | 0 | 33 | - | ok |
| life_options_cursor_move | 30.51 | 2:29 | 0 | 0 | 33 | - | ok |
| life_save_load | 30.17 | 2:89 | 0 | 0 | 33 | 180+287 | ok |
| life_save_load_cursor_move | 30.51 | 2:29 | 0 | 0 | 33 | - | ok |
| life_saving_the_save_freeze | 3.25 | 1:2 2:38 | 1 | 1 | 11600 | - | MISS |
| life_care_popup_feed | 30.25 | 2:59 | 0 | 0 | 33 | 180+287 | ok |
| life_care_reaction | 30.00 | 2:58 3:1 | 0 | 0 | 50 | - | ok |
| life_shop_food | 30.00 | 2:88 3:1 | 0 | 0 | 50 | 180+287 | ok |
| life_shop_cursor_move | 30.51 | 2:29 | 0 | 0 | 33 | - | ok |
| life_shop_quantity_popup | 30.00 | 2:58 3:1 | 0 | 0 | 50 | - | ok |
| life_shop_gifts_tab | 30.25 | 2:59 | 0 | 0 | 33 | - | ok |
| life_black_market | 30.17 | 2:89 | 0 | 0 | 33 | 180+287 | ok |
| life_cafe_counter | 30.17 | 2:89 | 0 | 0 | 33 | 180+287 | ok |
| life_news_help_wanted | 30.17 | 2:89 | 0 | 0 | 33 | 180+287 | ok |
| life_news_third_tab | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| life_graveyard | 30.17 | 2:89 | 0 | 0 | 33 | 180+287 | ok |
| life_unlocks | 30.00 | 2:88 3:1 | 0 | 0 | 50 | 180+287 | ok |
| life_unlocks_cursor_move | 30.00 | 2:28 3:1 | 0 | 0 | 50 | - | ok |
| life_unlocks_paints_tab | 30.25 | 2:59 | 0 | 0 | 33 | - | ok |
| life_citizen_picker | 30.00 | 2:88 3:1 | 0 | 0 | 50 | 180+287 | ok |
| life_citizen_picker_move | 30.51 | 2:29 | 0 | 0 | 33 | - | ok |

**story**

| window | fps (= logic) | frames by vsyncs | >33 ms | >50 ms | worst ms | overlay load ms | verdict |
|---|---:|---|---:|---:|---:|---:|---|
| story_first_boot_language_picker | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| story_language_picker_move | 30.51 | 2:29 | 0 | 0 | 33 | - | ok |
| story_first_boot_security_code | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| story_security_code_digit | 30.51 | 2:29 | 0 | 0 | 33 | - | ok |
| story_title_new_story_the_load | 8.78 | 2:39 68:1 140:1 | 2 | 2 | 2333 | 288+348+288+347+328+347+328 | MISS |
| story_prologue_cutscene | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| story_quest_log_story_tab | 30.17 | 2:89 | 0 | 0 | 33 | 348 | ok |
| story_quest_log_side_tab | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| story_quest_log_side_cursor_move | 30.51 | 2:29 | 0 | 0 | 33 | - | ok |
| story_quest_log_items_tab | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| story_quest_log_map_tab | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| story_quest_log_map_cursor_move | 30.51 | 2:29 | 0 | 0 | 33 | - | ok |
| story_quest_log_friends_tab | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| story_quest_log_trophies_tab | 30.17 | 2:89 | 0 | 0 | 33 | - | ok |
| story_cutscene_chapter_card_2 | 26.73 | 2:69 3:18 4:2 | 20 | 2 | 67 | 328 | MISS |
| story_cutscene_chapter_card_3 | 30.17 | 2:89 | 0 | 0 | 33 | 348+328 | ok |
| story_cutscene_chapter_card_4 | 30.17 | 2:89 | 0 | 0 | 33 | 348+328 | ok |
| story_cutscene_home_vignette | 30.17 | 2:89 | 0 | 0 | 33 | 348+328 | ok |
| story_cutscene_fog_vignette | 30.17 | 2:89 | 0 | 0 | 33 | 348+328 | ok |
| story_cutscene_eruption_vignette | 30.00 | 2:88 3:1 | 0 | 0 | 50 | 348+328 | ok |
| story_cutscene_vapula_vignette | 30.17 | 2:89 | 0 | 0 | 33 | 348+328 | ok |
| story_cutscene_after_a_press | 30.25 | 2:59 | 0 | 0 | 33 | - | ok |
| story_credits | 10.89 | 2:96 4:1 13:2 14:3 15:3 16:3 17:20 18:1 | 33 | 33 | 300 | 348 | MISS |
| story_credits_parade | 1.52 | 3:1 10:1 27:3 28:6 29:7 30:29 31:34 32:7 39:1 51:1 52:1 53:2 54:6 55:6 56:6 57:5 58:2 59:3 79:3 80:1 81:1 82:1 83:1 84:1 | 129 | 128 | 1400 | - | MISS |


## 3. Results: Dreamcast (Flycast)

Per group (every window not listed held 30.0 logic ticks/s with no tick over 33.3 ms of work):

| group | windows | ticks drawn / run | worst tick (scene) | windows not ok |
|---|---:|---|---|---|
| fair | 23 | 6040 of 6040 | 15.8 ms (MG_DERBY) | none |
| fishcook | 16 | 3335 of 3335 | 18.9 ms (COOKING) | none |
| library | 23 | 2631 of 2631 | 20.0 ms (LIB_ALMANAC) | none |
| action | 22 | 3258 of 3258 | 23.8 ms (WORKSHOP) | none |
| dream | 9 | 1482 of 1483 | 14.9 ms (DREAM) | none |
| house | 19 | 2136 of 2461 | 49.1 ms (HOME) | 14 of 19 (every room window; not the visit, HomeMart, Black Market): 21-115 ticks over 33 ms, worst 40-49 ms (table below) |
| studio | 9 | 1165 of 1165 | 18.2 ms (STUDIO) | none |
| creator | 25 | 2335 of 2335 | 16.0 ms (CREATOR) | none |
| life | 46 | 3701 of 3702 | 4473.9 ms (SAVELOAD) | life: new island -> hub (the load) (1 over 33 ms, worst 97.7 ms); life: saving (the save freeze) (1 over 33 ms, worst 4473.9 ms) |
| story | 24 | 3150 of 3205 | 99.7 ms (CUTSCENE) | story: title -> new story (the load) (1 over 33 ms, worst 99.7 ms); story: credits (21 over 33 ms, worst 35.5 ms); story: credits parade (72 over 33 ms, worst 62.2 ms) |

The house on the Dreamcast, every window (the core's weak room default until DC8 lands; room-test is not on the
Dreamcast, so the busy layout row is plain Live mode):

| window | logic t/s | ticks >33.3 ms | >50 ms | worst ms | verdict |
|---|---:|---:|---:|---:|---|
| house: build mode | 30.2 | 115 | 0 | 40.2 | MISS |
| house: build cursor move | 30.0 | 27 | 0 | 39.8 | MISS |
| house: catalog modal | 30.0 | 65 | 0 | 49.1 | MISS |
| house: catalog cursor move | 30.0 | 21 | 0 | 46.8 | MISS |
| house: held piece | 30.0 | 53 | 0 | 40.5 | MISS |
| house: rotate view | 30.0 | 26 | 0 | 40.0 | MISS |
| house: photo mode | 29.7 | 74 | 0 | 42.8 | MISS |
| house: photo snap | 30.0 | 25 | 0 | 46.6 | MISS |
| house: live mode | 30.0 | 105 | 0 | 43.6 | MISS |
| house: score panel | 30.0 | 69 | 0 | 46.1 | MISS |
| house: live, 4 chibis (room-test 1) | 30.0 | 105 | 0 | 43.6 | MISS |
| house: defend tutorial | 30.0 | 109 | 0 | 42.1 | MISS |
| house: defend siege wave 3 | 30.0 | 108 | 0 | 42.6 | MISS |
| house: defend siege freeze + double clock | 30.0 | 74 | 0 | 42.8 | MISS |
| house: visit (Nana) | 30.0 | 0 | 0 | <33.3 | ok |
| house: homemart | 30.0 | 0 | 0 | <33.3 | ok |
| house: homemart cursor move | 30.0 | 0 | 0 | <33.3 | ok |
| house: homemart season tab | 30.0 | 0 | 0 | <33.3 | ok |
| house: black market | 30.0 | 0 | 0 | <33.3 | ok |

## 4. What the profiles say (GBA, `make -C gba profile-speed2`)

Shares of CPU cycles over the window (overlay-aware names, SP1 T1). Seven causes cover nearly every miss:

| cause | where (GBA work ms) | profile evidence | why it is slow |
|---|---|---|---|
| **C1 the scene under a modal is redrawn and dimmed every frame** | house catalog 57.7 / Score panel 45.0; creator START menu 70; library cards (bestiary jar 50, almanac profile / face 57, weapons plan 66, item card 50, trophy page 47, gallery tiled 49, Field Guide page 43); studio quick fill 58, member card 54, citizen picker 62, song book 45; care popup 58, shop quantity 60, fishdex 56, cooking result card 54; add-member wizard 35; the fair title cards 56-61 | `vid_shade_words` (the dim, `ui_shade_screen_under`) 12-30% + the whole scene beneath (house `draw_region` / `roof_row` / `floor_row` 35%, library list + text) | the modal itself is 8-20 ms; the backdrop it hides is re-composed and re-shaded 30 times a second although it does not change. Exactly 15.0 / 20.0 fps: a steady 4- / 3-vblank frame |
| **C2 the 3D view keeps rendering under cards and menus** | Hell Breach intro card 94.5 ms (10 fps, logic 18), pause 104; arena intro 95, pause 101, results 106; Boss Rush / Horde / Mayhem cards and results 84-89; level select 50 (its preview) | `r3d_column_asm` 12-16%, `r3d_flat_loop` 10-19%, `ba_face` 6-9%, `r3d_sky_loop` 5-7%, plus `vid_shade_words` 13-18% | the combat renderer (~80 ms, SP8's) runs for a picture that stands still behind a card; the logic falls to 15-19 ticks/s, so a results screen runs in slow motion |
| **C3 static backdrops drawn as hundreds of rects every frame** | Fun Fair: Whack 57, Gallery 85 (logic 19), Dodge 39-43, Memory Hellfire 38; kitchen 38-54; dreams 60-80 (logic 18-25); cutscene cards and vignettes 41-93 (fog 93, logic 18); library lobby 41, jukebox 41; studio STAGE tab 36 | `vid_rect_narrow` 19-32%, `out_rect` 5-12%, `ellipse_draw` 3-19%, `nested_bands` 3-7%, `gba_udivmod` 3-7% | tents, stripes, bunting, kitchen tiles, dream corridors and skies are re-filled rect by rect; only a few sprites move over them |
| **C4 scaled / per-pixel art recomputed every frame** | Prize Wheel 151 ms (6.5 fps, logic 10.7); Derby 56-60 (`mg_blit2` 26%); Vapula vignette 76 (`vid_blit_c` 39%); weapons wing 35-66 (`vid_blit_c` 17-21%); home vignette 41 (`vid_blit` 54%) | the wheel: `__muldi3` 36% + `atan_oct` 20% (an `fx_atan2` per pixel of the disc); the others: 2x / scaled blits of art that does not change size | per-pixel maths and scaling that a table or a pre-scaled copy in ROM answers |
| **C5 chibi renders in bulk** | credits 136 ms avg, spikes 470-686 ms (5-7 fps); citizen picker move 97 ms frames; creator tab changes 55-67 ms frames; hat try-on 49 avg | `finish_marked` 14-17%, `capsule_plain` 6-9%, `sh_span` 8%, `pc_darken_part` 6-8%, `scan_transitions` 5% | the credits parade meets a new citizen every few ticks and draws each from scratch, several in one tick; SP2's slices and render-ahead do not reach the credits or these one-off pictures |
| **C6 linear scans per frame** | Unlocks gallery 67 ms (11 fps, logic 19), Paints tab 40 (15 fps) | `unlock_at` 25-37%, `unlock_owned_count` 5-8% | every visible tile asks for the n-th unlock of its category by scanning the whole unlock table (X3 indexed `unlock_find` only) |
| **C7 one-off freezes** | free-play save 11.4 s (GBA) / 11.6 s (PS1) / 4.5 s (DC); title -> New story 2.07 s; new island 2.0 s; workshop weapon composition 260 ms (3 frames a visit); house turn 106 ms; options Accessibility page turn 47 ms (`update_resident_ex` 12%: the sim's hour burst) | flash / memory card / VMU writes in one tick; town generation in one tick; `weld_gfx` `outline` 18% + `seg` 18% | work that blocks a single tick with nothing on screen moving |

## 5. Ranked misses (time spent there x how bad it feels)

Severity: logic below 30 (slow motion) > a steady low frame rate > periodic hitches > a few frames over.

### GBA

| rank | scene(s) | worst numbers | cause | packages |
|---:|---|---|---|---|
| 1 | Fun Fair: all seven title cards; Whack, Gallery, Derby, Dodge, Memory Hellfire play; the Prize Wheel | wheel 6.5 fps / logic 10.7; Gallery 11 / 19.4; Whack, Derby 15; title cards 15; Dodge 20-27 | C1, C3, C4 | S2-1, S2-3, S2-4 |
| 2 | Hell Breach / Demon Arena / arena-mode cards, pause menus and results; level select | 8.5-11 fps, logic 15-19; level select 15-19 fps | C2 | S2-2, S2-1 |
| 3 | modals everywhere: house catalog and Score panel, library cards and Field Guide pages, studio choir popups / picker / song book, care popup, shop quantity, fishdex, creator START menu, add-member wizard | 12-20 fps (logic 23-30) | C1 | S2-1 |
| 4 | dreams (every vignette, the nightmare, the wake-up choice) | 10.6-13 fps, logic 17.6-25 | C3 (+ dim) | S2-4, S2-1 |
| 5 | story cutscenes (chapter cards 2 / 4, home, fog, eruption, Vapula vignettes) and the credits | 10-23 fps (fog logic 18); credits 5-7 fps with 470-686 ms frames | C3, C4, C5 | S2-4, S2-5, S2-7 |
| 6 | kitchen (book, chop, stir, fry, bake, simmer, result, counter) | 17-22 fps | C3, C1 | S2-4, S2-1 |
| 7 | Unlocks gallery | 11 fps, logic 19 | C6 | S2-6 |
| 8 | library lobby, jukebox, weapons wing, Field Guide | 15-25 fps | C3, C4, text | S2-4, S2-8, S2-1 |
| 9 | workshop + welding | 19-23 fps, 260 ms freezes | C7, C4 | S2-9 |
| 10 | loads and saves: save 11.4 s, title -> New story 2.1 s, new island 2.0 s | single frozen frames | C7 | S2-10, G-LD |
| 11 | studio STAGE tab, creator tab changes / hat grid / swatch moves, picker moves, house turn and busy Live layout, Black Market, options Accessibility | 23-29 fps or 1-27 frames of 50-117 ms | C5, C3, C7 | S2-7, S2-11, S2-1 |

What already holds 30 on the GBA (no frame over 33 ms): Candy Catch (Sweet) and its results, fishing (pier, cast, reel,
cooler, night), Build / Live / Photo mode, Hold the Door tutorial, a visit, HomeMart, the library list pages, the
creator's tabs at rest and its grid, the family dashboard, play-as, all nine Options pages, Save / Load, news tabs,
graveyard, the quest log's six tabs (Map included), first-boot language and code screens, the prologue, the FILE tab
and templates.

### PS1 (Redux)

| rank | scene | numbers | cause | packages |
|---:|---|---|---|---|
| 1 | the credits parade | **1.5 fps**, every frame 450-1,400 ms (the opening credits 10.9 fps, 33 frames of 217-300 ms) | C5 (chibis in bulk; Redux CPU-bound) | S2-7 |
| 2 | Fun Fair: the PS1 **hangs** entering Vape Cloud Dodge (deterministic, frame 2671 of `speed2_fair`, after Candy Catch incl. its 1,500-tick session and Whack) | no frame for 900+ vsyncs | a bug, not speed | S2-12 |
| 3 | free-play save | one 11.6 s frame (the memory card write in one tick) | C7 | S2-10 |
| 4 | title -> New story; new island | New story: a 2.33 s and a 1.13 s frame + 7 overlay loads (288-348 ms each); new island: a 1.77 s frame + 4 overlay loads, then 40 frames at 50 ms | C7, overlays | S2-10, G-LD |
| 5 | Prize Wheel (intro and spin) | 22.4 fps, 59-83 frames at 50 ms | C4 (per-pixel atan) | S2-3 |
| 6 | Hell Breach intro card | 22.9 fps (55 frames at 50 ms) | C2 | S2-2 |
| 7 | chapter card 2 | 26.7 fps (20 frames at 50-67 ms) | C3 | S2-4 |
| 8 | workshop | 3 frames of 100 ms (the weapon composition) | C7 | S2-9 |
| 9 | house busy Live layout (room-test 1), creator hat try-on | 28.6-28.9 fps | C5 | S2-7, S2-11 |
| 10 | single 50-67 ms frames: nightmare vignettes and choice, library list moves, the kitchen counter, Mayhem (combat) | 29.5-29.9 fps | C5 / sim | S2-7 |
| - | every scene change | 260-630 ms of overlay loading (`OVL.BIN` from the drive, about 150 ms of it waiting for the drive) | loads | G-LD (fade over it) |

Everything else on the PS1 holds 30 with no frame over 33 ms: the other six minigames (Gallery and Derby included),
fishing and cooking, every library wing and the Field Guide, every other action card, menu and results screen,
dreams, the house (Build, catalog, Photo, Live, Score, Defend, visit, HomeMart), every creator tab and grid, the life
screens (family, play-as, Options, Save / Load, care, shops, news, graveyard, Unlocks, picker), the quest log,
cutscenes other than card 2, and every studio window.

### Dreamcast (Flycast)

Every 2D scene holds 30 with work under 24 ms (fair 15.8 ms max, creator 16.0, library 20.0). Misses: **the house**
(1,487 of 2,136 ticks over 33 ms, 2,136 of 2,461 ticks drawn: the room is still the core's weak default drawn
through ~6,100 2D prims a frame; DC8 is landing), **the credits** (72 ticks of 50-62 ms: 5,587 texture uploads and
4,038 evictions, the chibi texture cache thrashing), the save (one 4.47 s tick, the VMU write) and two single load
frames (new island 98 ms, title -> New story 100 ms).

## 6. Fix packages

Size: S (a day or less), M (a few days), L (a week+). Savings are per drawn frame on the named scenes, estimated
from the measured shares (share x frame time), not measured. "Exact" = every frame identical (`vhash-seq`, native
frame hashes, PS1 shots); "pixel-changing" needs the owner's yes (SPEED DEFAULTS already allow held frames, short
fades over loads and stand-ins).

| ID | what | size | files | expected saving | exact? | RAM |
|---|---|---|---|---|---|---|
| **S2-1** | **Modal hold** (C1): one core helper for every dimmed modal. When a modal opens over a scene whose picture does not change while it is up (the house, library lists, the studio, shops, the creator, cooking, fishing, the add-member wizard, minigame title cards' fair), draw the scene + `ui_shade_screen_under` once into both Mode 4 pages, `plat_hold_frame(1)` (U4, the pause panel's P2/P3 path), then redraw only the modal's rect each tick (its pop-in scale too: the hold starts when the pop ends). Scenes whose backdrop animates under the modal (a minigame's idle bobbing, the studio choir bounce) freeze it while the modal is up: that is the pixel-changing part, listed per scene | M | `core/src/ui.c` (`ui_dim_under_window_pop`, a `ui_modal_hold()` key), callers in `home_draw.c`, `lib_*.c`, `studio*.c`, `fp_shop.c`, `fp_resident.c`, `fishcook*.c`, `creator*.c`, `fp_family.c`, `mg.c` (title card); `gba/src/hold.c` (two-page capture) | -25..-45 ms: catalog 58 -> ~15, Score 45 -> ~12, library cards 43-66 -> ~15-25, studio popups 45-62 -> ~15, care / quantity 58-60 -> ~15, creator START 70 -> ~15, fair title cards 56-61 -> ~10: all to 30 fps | exact where the backdrop is static (most); pixel-changing (frozen idle motion) on the fair title cards and studio | 0 B (the pages are the buffer); PS1 / DC: weak default, they hold 30 already |
| **S2-2** | **3D held under cards** (C2): Hell Breach and arena intro cards, pause menus, results and death screens draw the 3D view once (it is paused or not started) and hold it under the card (S2-1's helper with the r3d frame as the backdrop); the level select's 3D preview is rendered once per level change into the held page and only re-rendered when the cursor moves | S/M | `core/src/levels_play.c`, `levels_select.c`, `arena.c`, `arena_modes.c`, `combat.c` (pause), `gba/src/hold.c` | cards / menus / results 84-106 ms -> ~10-20 ms: 9-11 fps -> 30, logic 15-19 -> 30; level select 50 -> ~25 ms; PS1 Breach card 22.9 -> 30 | exact (the view does not move under a paused / unstarted level; Mayhem's card shows a live clock? check: then the clock is drawn on top) | 0 B |
| **S2-3** | **Prize Wheel angle table** (C4): the wheel's per-pixel `fx_atan2` (`__muldi3` 36% + `atan_oct` 20%) becomes a ROM table of the angle byte for every (dx, dy) of one quadrant of the disc (built by a `tools/` generator at build time), mirrored for the other three; the static rim and stand drawn once and held | S | `core/src/mg_wheel.c`, `tools/` table generator, `core/gen` | -85 ms of 151: 6.5 -> ~20 fps; with the hold of the static parts -> 30; PS1 22.4 -> 30 | exact (the same angle per pixel: the table is made by the same `fx_atan2`) | ROM ~4-15 KB (32 MB free); 0 B RAM |
| **S2-4** | **Static backdrops kept** (C3): the Fun Fair stage, the kitchen, dream rooms / skies, cutscene card and vignette backgrounds, the library lobby and jukebox, the studio STAGE: the static layer is drawn once into both pages (or kept as a G7 retained panel), and each tick only restores the rects under what moved (from a recorded rect list replayed clipped to the dirty rect: SP's record/replay technique) and draws the moving sprites; divisions in those painters (`gba_udivmod` 3-7%) through reciprocal tables in ROM. The fair's moving actors (moles, candies, racers, targets) can go further as OBJ sprites like the PERFORM choir (G-K1 Option C) | L (one sub-roadmap, like ISLAND_SPEED, one scene family at a time: fair, kitchen, dreams, cutscenes, library) | `core/src/mg*.c`, `cook*.c`, `dream*.c`, `cutscene*.c`, `lib_lobby.c`, `lib_jukebox.c`, `studio_stage.c`, `gba/src/panel.c`, `obj.c` | `vid_rect_narrow` + `out_rect` + `ellipse_draw` + `nested_bands` ~35-55% of 38-93 ms: Whack 57 -> ~28, Gallery 85 -> ~40 (+ OBJs -> 30), Dodge 39-43 -> ~25, kitchen 38-54 -> ~25, dreams 60-80 -> ~30-40, fog vignette 93 -> ~45, lobby / jukebox 41 -> ~25 | exact (same pixels: the restored rects come from the same recorded calls) | GBA: rect lists ~1-3 KB per scene in the scene scratch arena (27 KB, owned by the scene while it runs); EWRAM 0 B resident |
| **S2-5** | **Pre-scaled art in ROM** (C4): the Derby racers (`mg_blit2`, 2x), the Vapula vignette's scaled sprite (`vid_blit_c` 39%), the weapons wing's scaled icons, the cutscene's 2x portraits, the welding sprites: scaled copies made at build time (the sizes used are few and fixed) and drawn with the plain `vid_blit` | M | `core/src/mg_derby.c`, `cutscene*.c`, `lib_weapons.c`, `weld_game.c`, an art generator in `tools/` | Derby -12..-15 ms (60 -> ~45, with S2-4 -> 30), Vapula -25 ms, weapons -6..-12 ms, welding -3 ms | exact (the generator uses the same nearest-neighbour step) | ROM only (tens of KB) |
| **S2-6** | **Unlocks gallery index** (C6): per category the ordered list of unlock ids and the owned count, built once when the category opens (or a ROM table per category, like X3), instead of `unlock_at(cat, n)` scanning the table per tile per frame | S | `core/src/unlocks*.c`, `fp_unlocks.c` | -20..-30 ms: 67 -> ~38 ms; Paints 40 -> ~18 ms -> 30 | exact | ROM table (X3 style) or 0.6 KB in the scene scratch |
| **S2-7** | **Chibis for crowds and one-offs** (C5): the credits parade's citizens through SP2's slice queue with render-ahead (the parade order is known: render the next citizens in the idle time of the frames before, never more than one new chibi a tick, a stand-in silhouette if one is late); the citizen picker's and the creator's big preview on a cursor move / tab change / try-on through the same slices with the previous picture shown meanwhile. Dreamcast: the credits' texture cache sized or keyed for the parade (5,587 uploads / 4,038 evictions) | M | `core/src/credits*.c`, `portrait*.c`, `fp_pick*.c`, `creator*.c`; `dc/src/texcache.c` | GBA credits 136 ms avg / 686 ms frames -> ~35 ms, no frame over 50; PS1 parade 1.5 fps -> 30 (no more than one new chibi a tick); picker / creator 55-97 ms frames -> under 33 | pixel-changing for 1-3 ticks per new chibi (stand-in / previous picture), exact with render-ahead in the credits | 0 B new (SP2's queue and pool) |
| **S2-8** | **Text pages as kept panels**: the Field Guide page (43 ms, `wrap_core` 5% + `vid_text` 14%: wrapped every frame), the library item / trophy cards and the almanac profile text, kept as G7 retained panels while the page does not change | S | `core/src/lib_help.c`, `lib_*.c` | -10..-15 ms (with S2-1): Field Guide 43 -> ~25 ms -> 30 | exact | 0 B (panels live in the pages) |
| **S2-9** | **Workshop**: the weapon composition (`weld_gfx` `outline` + `seg`, 260 ms in one tick, 3 times a visit on GBA, 100 ms on PS1) sliced across ticks with the old picture shown meanwhile, and composed ahead for the part under the cursor's neighbours; the part list as a kept panel; the welding minigame's static bench held (S2-4) and its sprites pre-scaled (S2-5) | M | `core/src/weld_gfx.c`, `workshop.c`, `weld_game.c` | the 260 ms frames -> under 33 (1-3 ticks of the old weapon picture); workshop 45 -> ~25 ms, welding 38 -> ~28 ms | pixel-changing for the slice ticks; the rest exact | GBA: the composition's state in the existing 12 KB view slots |
| **S2-10** | **Saves and loads without a frozen screen** (C7): the free-play save streams a few KB a tick (flash sector erase + program per tick on the GBA, the memory card's 128 B sectors on the PS1, the VMU blocks on the DC) behind an animated "Saving..." card; town generation for a new island / New story spread over the frames of a short fade (G-LD from audit 1, with the island's 1,000-citizen `town_new` split by citizen) | M | `core/src/save*.c`, `gba/src/flash*.c`, `ps1/src/memcard*.c`, `dc/src/vmusave.c`, `core/src/town.c`, `fp_new_island.c`, `fp_title.c` | 11.4 s (GBA) / 11.6 s (PS1) / 4.5 s (DC) single frames -> the same time with the card animating at 30; 2.0-2.1 s loads -> a 30 fps fade | pixel-changing (a card / fade; the saved bytes identical: `test-coresave` and the PS1 memcard check compare them) | GBA: the stream state is ~16 B; no new buffer (the arena already holds the stream window) |
| **S2-11** | **House leftovers**: the view turn's recompose (106 ms: one 117 ms frame) built over 2-3 ticks in the hidden page while the shown page holds the old view (I10's `plat_build_frame`), and the busy Live layout's `draw_region` (18%, 27 frames at 50 ms with 4 walkers) restored per walker rect instead of per row | S/M | `core/src/home_draw.c`, `gba/src/room.c` | turn: no frame over 50; busy Live 32 -> ~26 ms | turn pixel-changing (old view 2 ticks longer); Live exact | 0 B |
| **S2-12** | **PS1 hang entering Vape Cloud Dodge** (bug): deterministic in `speed2_fair` at frame 2671 (Catch with its 1,500-tick session, then Whack, then `push MG_DODGE 1`); Catch alone then Dodge, or Whack then Dodge, do not hang. Bisect the minigame arena / overlay state (the `minigame` group is reloaded per game; `freeplay` 102 KB too), add the sequence as a PS1 test | S/M | `core/src/mg*.c`, `ps1/src/overlay.c` | a hang -> 30 fps (the PS1 Dodge measures 30.17 alone) | n/a | - |
| **S2-13** | Tooling (landed with this audit): the GBA harness `gaps` histogram, `tools/speed2.py`, the speed2 scripts and `test-speed2` / `profile-speed2`, `ps1/harness/speed2.sh`, `dc/harness/speed2.sh`. Next: `expect-fps` floors at 85% once each package lands (as G1 did), and a per-window drawn count on the Dreamcast (dcrun prints `game drew` per run only) | S | `gba/harness`, `tools/speed2.py`, `dc/harness/dcrun.c` | 0 ms | n/a | 0 |

### Suggested order

1. **S2-1 modal hold** and **S2-2 3D held under cards** first: one helper, exact on most scenes, and they take about
   40 of the 118 GBA misses (122 less the four combat reference windows) to 30 (house, library cards, studio, shops, creator, cooking cards, action cards and
   menus). **S2-6** (Unlocks) and **S2-3** (wheel) are small, exact, and lift the two worst menu numbers.
2. **S2-12** (the PS1 hang) as a bug fix, whenever the minigames are touched; **S2-7** early for the credits
   (the worst number on the PS1 and the second worst on the GBA).
3. **S2-4** static backdrops, one family at a time (fair, kitchen, dreams, cutscenes, library), then **S2-5** pre-scaled art
   where a family is still over 33 ms.
4. **S2-7** chibis (credits, picker, creator) on SP2's queue; **S2-9** workshop; **S2-8** text panels.
5. Owner calls (pixel-changing): S2-10's saving card and load fades, S2-7's stand-ins, S2-9's slice ticks, S2-11's turn.

## 7. RAM and ROM

* **GBA EWRAM**: 259,444 of 262,144 B used: **2,700 B free** (memreport on `d875604`). Nothing above adds resident
  EWRAM: S2-1 / S2-2 use the Mode 4 pages themselves, S2-4's rect lists live in the scene scratch arena (26,992 B,
  owned by the running scene), S2-6 is a ROM table or scene scratch.
* **GBA IWRAM**: resident 11,176 B + overlay slot 10,384 B + bss 4,084 B; 6,356 B of main stack. Overlay groups: 3D 10,384
  (0 free), 2D 8,592 (1,792 free), room 3,248 (7,136 free). An ARM span-restore loop for S2-4 fits the 2D group's 1,792 B
  (it runs in 2D scenes) or the room group's 7,136 B; nothing goes resident.
* **GBA ROM**: 4,534,084 B of 32 MB: S2-3's angle table, S2-5's pre-scaled art and S2-6's index cost nothing that matters.
* **PS1 RAM**: `elf2exe` on this tree: EXE 1,254 KB + BSS 534 KB, **68 KB below the stack top: already 4 KB under the
  72 KB plan gate** (d875604's message says 70 KB; this build's link says 68). No PS1 package above adds resident
  RAM: S2-2 and S2-12 are core / overlay code (scene overlay groups), S2-10's stream state is a few bytes in the save
  module's existing BSS. The PS1 needs none of S2-1, S2-3..S2-8 for its frame rate (it holds 30 there).
* **Dreamcast**: no constraint met yet (16 MB); S2-7's texture cache keying is the only DC item besides DC8's house.

## 8. What was not measured, and why

* **Hell Breach results card** (the S/A/B/C tally): it needs a cleared level; no script reaches it in a bounded time
  (the demo autopilot does not finish a level). The arena and arena-mode results (same drawing path over the 3D view)
  are measured; S2-2 covers both.
* **The graveyard with graves**: a fresh island has none and the runner's way to make one (`type` a name) is not on
  the GBA; the empty graveyard holds 30.
* **Title first boot as a console boots it**: the GBA keeps no config record and the harnesses skip the picker; the
  picker and the security code were pushed directly (both hold 30 everywhere).
* **Tip cards** over the island hub: measured by `make -C gba test-tips` (CL4: 20.7 fps with the card up), and the
  hub itself is SP3's; the Field Guide is in `speed2_library`.
* **Dreamcast per-window drawn fps**: dcrun prints drawn ticks per run, not per window; the table gives logic ticks/s
  and over-budget ticks per window. The house on DC is the core's weak default until DC8 lands, so its numbers are a
  placeholder's.
* **Real hardware**: emulators only (libmgba, PCSX-Redux, Flycast). The GBA save time is mGBA's flash timing model;
  a real cartridge's sector erase may be shorter, the freeze is real either way.

## Files

* This report; raw logs in `logs2/` next to it: `gba_speed2_*.log` + `gba_speed2_*_prof.log` (GBA tables and profiles),
  `ps1/<group>/tty.log` (PS1), `dc/<group>.log` (Dreamcast), `sheet_*.png` (contact sheets).
* Scripts and tools (committed): `gba/harness/scripts/speed2_*.txt`, `tools/speed2.py`, `gba/Makefile` targets
  `test-speed2` / `profile-speed2`, `gba/harness/harness.c` (`gaps` line), `ps1/harness/speed2.sh`,
  `dc/harness/speed2.sh`, notes in `docs/GBA.md`, `docs/PS1.md`, `docs/DREAMCAST.md` ("Speed tour 2").
