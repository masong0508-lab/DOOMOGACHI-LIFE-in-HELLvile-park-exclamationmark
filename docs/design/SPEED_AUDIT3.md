# BOREagotchi speed audit 3: what is still not a steady 30 fps (measure and plan)

Date: 2026-11-12 / 13. Tree measured: `origin/claude/nice-allen-gr6czm` at `89a28b56` (after S2-1..S2-8, S2-12,
S2-13 and the sticks). No game code was changed. Owner goal: a steady 30 fps on every screen and platform, no
hitches; combat stays as it is. Speed audit 2 is `held/SPEED_AUDIT2.md`.

**In flight while this was measured (not planned again here):** PS1-RAM (PS1 resident memory), S2-9 (workshop and
welding freezes), S2-10 (saves and loads without a frozen screen). Their windows are in the tables and marked.

## 0. In short

| platform | windows | combat (reference) | OK: 30 fps, no frame over 33 ms | MISS |
|---|---:|---:|---:|---:|
| GBA (libmgba) | 311 | 11 | **155** | **145** (slow 25, low 57, hitch 31, 1x 32) |
| PS1 (PCSX-Redux) | 247 | 8 | **221** | **18** (slow 7, hitch 2, 1x 9); 6 of them are in flight |
| Dreamcast (Flycast) | 218 | 4 (no 3D view yet: DC6) | **211** | **3** (the save and two loads: in flight) |

A window is OK only when logic and drawn fps are 30 and no presented frame took over 33 ms. MISS classes:
**slow** = the game logic itself under 30 ticks/s (slow motion); **low** = a steady low frame rate (a quarter or
more of the frames over 33 ms); **hitch** = 3 or more long frames in the window; **1x** = one or two long frames.

GBA: of the 214 speed tour 2 windows (combat aside) 120 are OK. Audit 2 had 76. 44 windows went from miss or
hitch to OK. One went the other way (the shop's gifts tab: the sim's 15-minute pass fell into its window).

**The five worst on the GBA** (not counting what is in flight):

1. **Shooting Gallery**: 10.5-11.0 drawn fps, logic 18.5-19.4 ticks/s (slow motion), 85-87 ms a frame.
2. **Dreams**: every vignette 10.6-13.2 fps, logic 17.6-24.5; the wake-up choice 10.6 fps.
3. **The fog vignette** (story cutscene): 10.0 fps, logic 18.4, 93 ms a frame.
4. **The concert**: band 13.2 fps (logic 23.6), setup screen 14.5, choir 15.0, solo 22.2.
5. **The sim's 15-minute pass**: every 37.7 s about 17 frames in a row of 50-67 ms, on every free-play screen
   (shops, picker, pet, apartments, Options). Small to fix, and it is everywhere.

Close behind: the kitchen (17-22 fps on all nine windows), the Fishdex and chapter card 2 (15 fps), whole chibi
renders inside one frame (95-325 ms frames on list moves and scene entries), the exorcism (19-23 fps).

In flight and still the longest frames: the save (11.4 s), title to New story (2.07 s), new island (1.99 s),
the workshop (three 260 ms frames a visit).

**PS1:** what is not in flight is small: the busy house layout 28.9 fps, the creator's hat try-on 28.6 and grid
scroll 29.4, single 50-67 ms frames in five scenes, and 98-502 ms of overlay reading at every scene change.
The Dodge hang, the credits (1.5 fps), the wheel, the Hell Breach card and chapter card 2 of audit 2 all hold 30.
The EXE ends 64 KB below the stack top with the boot FMV (gate 72 KB; PS1-RAM is in flight).

**Dreamcast:** every 2D scene holds 30, the house included (DC8). Only the save (4.47 s) and two load frames
(98-100 ms) miss, and S2-10 owns them. Combat is not built there yet (DC6).

## 1. Method and versions

| platform | build | runner | commands |
|---|---|---|---|
| GBA | `make -C gba -j2` at `89a28b56`: ROM 8,004,784 B, EWRAM 1,788 B free, main stack 6,308 B (arm-none-eabi-gcc 13.2.1) | libmgba 0.10.2 (Ubuntu 0.10.2+dfsg-1.1build3), `gba/build/harness` | `make -C gba test-speed2` (218 windows), `test-speed SPEED_GATES=0`, `profile-all`, `test-life`, `profile-exorcism`, `profile-concert`, `profile-creator`, `test-creator`, `profile-studio`, `profile-sung`, `profile-title`, `profile-pause`, `profile-invaded`, `test-tips`, `test-story`, `test-home`, `test-3d`, `profile-speed2`, and `sp4_tour.txt` by hand |
| PS1 | `cmake --preset ps1 -DDOOMO_INTRO_REQUIRED=ON`, `cmake --build --preset ps1 -j2` (mipsel-linux-gnu-gcc 12.4.0, PSn00bSDK). EXE 1,258 KB + BSS 534 KB, ends `0x801efe60`: **64 KB (65,936 B) below the stack top with the boot FMV**, 8 KB under the 72 KB gate (PS1-RAM is in flight) | PCSX-Redux at `ee9be68de5a482e7fea8eff7f4b1ddbc0f43b81e` (`/opt/ps1emu/redux.commit`) + OpenBIOS | `sh ps1/harness/speed2.sh OUT` (one group at a time), `PROF=1` for the groups with misses, the `perf_*.txt` scripts one by one (the same list as `perfall.sh`, plus life, exorcism phases, combat, `creator_speed.txt`) |
| Dreamcast | `make -C dc -j2` (KallistiOS, sh-elf gcc), `make -C dc/harness` | Flycast libretro at the pin `e7085e20fe8d81257e11aea5f19a9c1ca2b1bd55` (`dc/harness/build_flycast.sh`) | `sh dc/harness/speed2.sh OUT` |

How to read the numbers (as in audit 2):

* GBA "frames over 33 / 50 ms" come from the harness `gaps` line (the vblanks each presented frame took), not from
  the work per tick. GBA logic stays 30 ticks/s while no frame takes 5 vblanks or more.
* A window is `fps start` / `wait N` / `fps LABEL` with `sync on` and `seed 1`: the GBA numbers repeat exactly.
* PS1: one tick a frame, so fps is also the game speed. PCSX-Redux charges 1.5-2x a console's cycles for CPU-bound
  code. One 3-vsync frame at a window's start is the lockstep resume and is not counted.
* Dreamcast: logic ticks/s per window and the ticks whose work was over 33.3 ms. Flycast is not cycle-accurate.
* Profiles: `make -C gba profile-speed2` samples the ticks **after** a window. For every GBA miss this audit also
  profiled the window's own ticks (the script cut at the window, its `wait N` replaced by `profile`, played with
  `--worklog`, so the harness splits slow frames from normal ones). "ms a frame" below is a function's share of the
  cycles times the frame's length. Scripts for this are in the evidence folder (`tools/winprof.py`).
* The settle before a window is not in the tables. Long frames found there (scene entries, the second after a
  tab change) are listed in 3.4.

## 2. Results: every window

Columns: logic ticks/s, drawn fps, CPU work per drawn tick (avg / max), frames over 33 ms / over 50 ms, the worst
presented frame, audit 2's drawn fps (and logic) for the same window ("new": added since; "-": not in audit 2),
the verdict.

### 2.1 GBA

**fair** (speed tour 2: `speed2_fair.txt`)

| window | logic t/s | drawn fps | work avg / max ms | >33 / >50 ms | worst ms | audit 2 fps (logic) | verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| fair: catch title | 30.0 | 30.0 | 1.0 / 2.7 | 0 / 0 | 34 | 15.0 (30.0) | OK |
| fair: catch play sweet | 30.0 | 30.0 | 27.8 / 29.4 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| fair: catch results | 30.0 | 30.0 | 27.7 / 30.6 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| fair: catch play hellfire | 30.0 | 29.5 | 30.8 / 36.8 | 3 / 0 | 50 | 29.5 (30.0) | MISS hitch |
| fair: whack title | 30.0 | 30.0 | 1.0 / 1.4 | 0 / 0 | 34 | 15.0 (30.0) | OK |
| fair: whack play sweet | 29.9 | 20.9 | 40.2 / 61.7 | 73 / 5 | 67 | 15.1 (30.1) | MISS low |
| fair: whack play hellfire | 29.9 | 20.0 | 44.9 / 69.8 | 79 / 8 | 67 | 15.1 (29.7) | MISS low |
| fair: dodge title | 30.0 | 30.0 | 1.0 / 2.4 | 0 / 0 | 34 | 15.0 (30.0) | OK |
| fair: dodge play sweet | 29.9 | 22.5 | 38.6 / 45.8 | 64 / 0 | 50 | 22.5 (29.9) | MISS low |
| fair: dodge play late | 30.2 | 27.2 | 33.6 / 59.4 | 24 / 1 | 67 | 27.1 (30.1) | MISS hitch |
| fair: dodge play hellfire | 29.9 | 20.7 | 42.1 / 48.2 | 82 / 0 | 50 | 20.5 (30.0) | MISS low |
| fair: gallery title | 30.0 | 30.0 | 1.0 / 2.4 | 0 / 0 | 34 | 15.0 (30.0) | OK |
| fair: gallery play sweet | 19.4 | 11.0 | 85.2 / 88.9 | 74 / 74 | 100 | 11.0 (19.4) | MISS slow |
| fair: gallery play hellfire | 18.5 | 10.5 | 87.3 / 94.3 | 74 / 74 | 100 | 10.5 (18.5) | MISS slow |
| fair: derby title | 30.0 | 30.0 | 1.0 / 2.4 | 0 / 0 | 34 | 15.0 (30.0) | OK |
| fair: derby race sweet | 29.9 | 22.1 | 38.0 / 43.5 | 69 / 0 | 50 | 15.0 (30.0) | MISS low |
| fair: derby race mid | 29.9 | 22.1 | 37.5 / 42.1 | 67 / 0 | 50 | 15.3 (30.6) | MISS low |
| fair: derby race hellfire | 30.0 | 22.6 | 37.6 / 42.8 | 66 / 0 | 50 | 15.0 (30.0) | MISS low |
| fair: memory title | 30.0 | 30.0 | 1.0 / 1.4 | 0 / 0 | 34 | 15.0 (30.0) | OK |
| fair: memory play sweet | 30.0 | 29.5 | 32.1 / 42.9 | 4 / 0 | 50 | 29.5 (30.0) | MISS hitch |
| fair: memory play hellfire | 30.0 | 21.5 | 38.4 / 42.7 | 74 / 0 | 50 | 21.5 (30.0) | MISS low |
| fair: wheel intro | 30.0 | 30.0 | 26.9 / 27.3 | 0 / 0 | 34 | 6.4 (10.7) | OK |
| fair: wheel spin | 29.9 | 29.9 | 27.2 / 60.7 | 1 / 1 | 67 | 6.5 (10.9) | MISS 1x |

**fishcook** (speed tour 2: `speed2_fishcook.txt`)

| window | logic t/s | drawn fps | work avg / max ms | >33 / >50 ms | worst ms | audit 2 fps (logic) | verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| fish: pier day | 30.0 | 30.0 | 28.0 / 29.9 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| fish: bot cast and wait | 29.9 | 29.7 | 26.1 / 45.6 | 3 / 0 | 50 | 29.7 (29.9) | MISS hitch |
| fish: reel | 30.0 | 30.0 | 28.6 / 29.3 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| fish: catch card | 30.7 | 25.6 | 31.9 / 62.5 | 14 / 14 | 84 | 25.5 (31.0) | MISS hitch |
| fish: fishdex | 30.0 | 15.0 | 56.0 / 57.7 | 45 / 45 | 67 | 15.0 (30.0) | MISS low |
| fish: cooler | 30.0 | 30.0 | 29.7 / 30.7 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| fish: night reel | 30.0 | 30.0 | 26.6 / 35.8 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| cook: recipe book | 30.0 | 20.0 | 48.6 / 49.7 | 60 / 0 | 50 | 20.0 (30.0) | MISS low |
| cook: chop | 30.0 | 21.9 | 38.5 / 52.3 | 68 / 0 | 50 | 22.2 (30.0) | MISS low |
| cook: stir | 29.9 | 20.0 | 42.7 / 54.1 | 85 / 0 | 50 | 20.3 (30.0) | MISS low |
| cook: fry | 30.0 | 20.3 | 38.7 / 41.2 | 84 / 0 | 50 | 20.5 (30.0) | MISS low |
| cook: result card | 30.0 | 17.0 | 54.1 / 54.5 | 51 / 27 | 67 | 16.9 (29.8) | MISS low |
| cook: bake | 30.0 | 20.8 | 40.8 / 57.4 | 79 / 1 | 67 | 20.8 (30.0) | MISS low |
| cook: simmer | 30.0 | 22.4 | 39.0 / 77.5 | 63 / 2 | 84 | 22.4 (30.0) | MISS low |
| cook: counter | 30.0 | 18.7 | 45.2 / 79.2 | 53 / 10 | 84 | 19.0 (30.0) | MISS low |
| cook: serving | 30.0 | 19.0 | 44.2 / 73.1 | 38 / 5 | 84 | 19.0 (30.0) | MISS low |

**library** (speed tour 2: `speed2_library.txt`)

| window | logic t/s | drawn fps | work avg / max ms | >33 / >50 ms | worst ms | audit 2 fps (logic) | verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| library: lobby | 30.0 | 19.8 | 41.3 / 70.1 | 86 / 2 | 67 | 19.8 (30.0) | MISS low |
| library: lobby shelf move | 30.5 | 19.3 | 39.8 / 49.0 | 19 / 0 | 50 | 19.3 (30.5) | MISS low |
| library: bestiary list | 30.0 | 30.0 | 27.5 / 29.2 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| library: bestiary jar | 30.0 | 30.0 | 17.9 / 19.8 | 0 / 0 | 34 | 19.6 (29.8) | OK |
| library: almanac list | 30.0 | 30.0 | 32.4 / 34.2 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| library: almanac list move | 30.5 | 25.4 | 34.7 / 44.5 | 7 / 0 | 50 | 25.9 (31.0) | MISS low |
| library: almanac profile | 30.0 | 30.0 | 25.2 / 36.4 | 0 / 0 | 34 | 15.7 (30.7) | OK |
| library: almanac face | 30.0 | 30.0 | 27.3 / 32.5 | 0 / 0 | 34 | 15.6 (30.5) | OK |
| library: weapons frames | 30.0 | 30.0 | 31.6 / 33.1 | 0 / 0 | 34 | 24.9 (29.8) | OK |
| library: weapons plan | 30.0 | 30.0 | 21.5 / 22.7 | 0 / 0 | 34 | 15.0 (30.0) | OK |
| library: items | 30.0 | 30.0 | 30.4 / 31.8 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| library: items card | 30.0 | 30.0 | 15.3 / 16.5 | 0 / 0 | 34 | 19.2 (29.8) | OK |
| library: trophies | 30.0 | 30.0 | 29.9 / 31.3 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| library: trophy page | 30.0 | 30.0 | 12.7 / 13.9 | 0 / 0 | 34 | 20.0 (30.0) | OK |
| library: jukebox | 30.0 | 20.0 | 40.8 / 42.8 | 60 / 0 | 50 | 20.0 (30.0) | MISS low |
| library: jukebox playing | 30.0 | 19.8 | 41.7 / 42.9 | 86 / 0 | 50 | 19.8 (30.0) | MISS low |
| library: gallery textures | 30.0 | 30.0 | 31.3 / 35.3 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| library: gallery tiled | 30.0 | 30.0 | 16.2 / 17.5 | 0 / 0 | 34 | 20.0 (30.0) | OK |
| library: gallery sky | 30.0 | 30.0 | 27.6 / 30.3 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| library: field guide contents | 30.0 | 30.0 | 25.2 / 26.6 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| library: field guide page | 30.0 | 30.0 | 8.7 / 9.9 | 0 / 0 | 34 | 20.0 (30.0) | OK |
| library: field guide page turn | 30.8 | 30.8 | 10.6 / 19.3 | 0 / 0 | 34 | 20.0 (30.8) | OK |
| library: field guide scroll | 30.0 | 30.0 | 8.6 / 8.6 | 0 / 0 | 34 | 20.0 (30.8) | OK |

**action** (speed tour 2: `speed2_action.txt`)

| window | logic t/s | drawn fps | work avg / max ms | >33 / >50 ms | worst ms | audit 2 fps (logic) | verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| action: level select | 30.1 | 29.7 | 33.3 / 34.0 | 3 / 0 | 50 | 18.8 (30.2) | MISS hitch |
| action: level select move | 30.8 | 16.9 | 50.4 / 83.5 | 11 / 2 | 100 | 15.0 (30.0) | MISS low |
| action: breach intro card | 30.0 | 30.0 | 7.2 / 8.5 | 0 / 0 | 34 | 10.2 (18.3) | OK |
| action: breach play (combat, reference) | 29.7 | 14.5 | 52.9 / 66.0 | 44 / 44 | 84 | 11.3 (19.1) | combat (ref) |
| action: breach pause menu | 30.2 | 30.2 | 14.5 / 16.9 | 0 / 0 | 34 | 8.8 (15.9) | OK |
| action: level select after a level | 30.0 | 30.0 | 33.3 / 34.6 | 2 / 0 | 50 | 17.9 (29.8) | MISS 1x |
| action: arena intro card | 30.0 | 30.0 | 26.7 / 28.0 | 0 / 0 | 34 | 9.4 (17.0) | OK |
| action: arena fight (combat, reference) | 30.0 | 14.3 | 53.9 / 100.6 | 43 / 43 | 117 | 9.0 (16.2) | combat (ref) |
| action: arena pause menu | 30.0 | 30.0 | 5.5 / 6.9 | 0 / 0 | 34 | 8.5 (15.5) | OK |
| action: arena results | 30.0 | 30.0 | 11.1 / 12.3 | 0 / 0 | 34 | 8.6 (15.0) | OK |
| action: boss rush intro card | 30.0 | 30.0 | 28.1 / 29.4 | 0 / 0 | 34 | 11.1 (19.2) | OK |
| action: boss rush round 1 (combat, reference) | 30.0 | 14.7 | 57.3 / 73.8 | 44 / 44 | 84 | 9.8 (17.9) | combat (ref) |
| action: boss rush results | 30.0 | 30.0 | 28.6 / 30.7 | 0 / 0 | 34 | 10.0 (18.4) | OK |
| action: horde intro card | 30.0 | 30.0 | 28.8 / 30.4 | 0 / 0 | 34 | 11.1 (18.8) | OK |
| action: horde results | 30.0 | 30.0 | 29.7 / 31.1 | 0 / 0 | 34 | 10.3 (17.1) | OK |
| action: mayhem intro card | 30.0 | 30.0 | 30.4 / 31.9 | 0 / 0 | 34 | 10.3 (17.1) | OK |
| action: mayhem chaos (combat, reference) | 21.4 | 8.8 | 81.4 / 119.0 | 37 / 37 | 167 | 6.6 (11.0) | combat (ref) |
| action: workshop | 25.6 | 18.7 | 42.8 / 260.4 | 78 / 3 | 268 | 18.9 (25.3) | MISS slow |
| action: workshop part change | 30.5 | 23.4 | 37.9 / 39.4 | 15 / 0 | 50 | 23.4 (30.5) | MISS low |
| action: workshop armory | 30.0 | 26.7 | 34.2 / 35.1 | 20 / 0 | 50 | 27.0 (30.0) | MISS low |
| action: weld intro card | 30.0 | 30.0 | 28.3 / 30.0 | 0 / 0 | 34 | 22.7 (30.0) | OK |
| action: welding | 29.9 | 29.9 | 28.8 / 35.1 | 0 / 0 | 34 | 21.8 (30.1) | OK |

**dream** (speed tour 2: `speed2_dream.txt`)

| window | logic t/s | drawn fps | work avg / max ms | >33 / >50 ms | worst ms | audit 2 fps (logic) | verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| dream: bedroom | 30.0 | 30.0 | 24.5 / 30.9 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| dream: thought bubble | 28.1 | 22.5 | 32.3 / 78.8 | 14 / 12 | 84 | 22.3 (27.9) | MISS slow |
| dream: vignette 1 cute | 24.5 | 13.2 | 61.3 / 63.6 | 59 / 59 | 84 | 13.3 (24.7) | MISS slow |
| dream: vignette 1 twist | 24.0 | 13.0 | 62.8 / 68.6 | 54 / 54 | 84 | 12.9 (24.3) | MISS slow |
| dream: vignette 2 | 23.9 | 12.7 | 63.3 / 66.6 | 69 / 69 | 84 | 12.8 (24.1) | MISS slow |
| dream: nightmare vignette 1 | 20.1 | 11.4 | 70.8 / 89.3 | 74 / 74 | 117 | 11.7 (20.9) | MISS slow |
| dream: nightmare vignette 2 | 22.5 | 12.3 | 64.6 / 79.1 | 71 / 71 | 100 | 12.3 (22.6) | MISS slow |
| dream: nightmare vignette 3 (cute, then red) | 17.6 | 10.6 | 81.1 / 85.5 | 60 / 60 | 100 | 10.6 (17.6) | MISS slow |
| dream: wake-up choice | 17.6 | 10.6 | 80.8 / 86.5 | 36 / 36 | 100 | 10.6 (17.6) | MISS slow |

**house** (speed tour 2: `speed2_house.txt`)

| window | logic t/s | drawn fps | work avg / max ms | >33 / >50 ms | worst ms | audit 2 fps (logic) | verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| house: build mode | 30.0 | 30.0 | 12.3 / 16.3 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| house: build cursor move | 30.0 | 30.0 | 11.1 / 13.0 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| house: catalog modal | 30.0 | 30.0 | 17.9 / 18.5 | 0 / 0 | 34 | 15.0 (30.0) | OK |
| house: catalog cursor move | 30.0 | 30.0 | 18.2 / 18.4 | 0 / 0 | 34 | 15.0 (30.0) | OK |
| house: held piece | 30.3 | 30.3 | 13.4 / 17.2 | 0 / 0 | 34 | 30.3 (30.3) | OK |
| house: rotate view | 30.5 | 27.5 | 14.7 / 106.2 | 1 / 1 | 117 | 27.5 (29.5) | MISS 1x |
| house: photo mode | 29.8 | 29.8 | 16.2 / 19.9 | 0 / 0 | 34 | 29.8 (29.8) | OK |
| house: photo snap | 30.0 | 30.0 | 11.3 / 14.8 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| house: live mode | 30.0 | 30.0 | 10.2 / 11.5 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| house: score panel | 30.0 | 30.0 | 13.1 / 14.4 | 0 / 0 | 34 | 20.0 (30.0) | OK |
| house: live, 4 chibis (room-test 1) | 30.0 | 26.8 | 32.4 / 47.7 | 27 / 0 | 50 | 26.8 (30.0) | MISS hitch |
| house: defend tutorial | 30.0 | 30.0 | 28.8 / 35.6 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| house: defend siege wave 3 | 30.0 | 29.5 | 31.4 / 37.2 | 4 / 0 | 50 | 29.5 (30.0) | MISS hitch |
| house: defend siege freeze + double clock | 30.0 | 30.0 | 30.4 / 35.4 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| house: visit (Nana) | 30.0 | 30.0 | 12.9 / 16.0 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| house: homemart | 30.0 | 30.0 | 29.1 / 30.0 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| house: homemart cursor move | 30.0 | 30.0 | 31.6 / 32.0 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| house: homemart season tab | 30.0 | 30.0 | 26.9 / 28.4 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| house: black market | 30.0 | 30.0 | 33.2 / 34.6 | 0 / 0 | 34 | 29.7 (30.0) | OK |

**studio** (speed tour 2: `speed2_studio.txt`)

| window | logic t/s | drawn fps | work avg / max ms | >33 / >50 ms | worst ms | audit 2 fps (logic) | verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| studio: choir quick fill | 30.0 | 30.0 | 9.4 / 9.4 | 0 / 0 | 34 | 15.0 (30.0) | OK |
| studio: choir member card | 30.0 | 30.0 | 6.3 / 6.3 | 0 / 0 | 34 | 15.0 (30.0) | OK |
| studio: citizen picker | 30.7 | 29.7 | 17.6 / 28.9 | 1 / 0 | 50 | 14.4 (28.1) | MISS 1x |
| studio: citizen picker move | 30.5 | 28.5 | 22.3 / 44.5 | 2 / 0 | 50 | 12.6 (23.7) | MISS 1x |
| studio: stage tab | 30.0 | 22.6 | 36.3 / 38.4 | 64 / 0 | 50 | 22.8 (30.0) | MISS low |
| studio: file tab | 30.0 | 30.0 | 21.5 / 22.9 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| studio: song book | 30.0 | 30.0 | 4.1 / 4.1 | 0 / 0 | 34 | 20.0 (30.0) | OK |
| studio: templates | 30.2 | 29.8 | 29.1 / 36.0 | 1 / 0 | 50 | 29.8 (30.2) | MISS 1x |
| studio: templates move | 30.0 | 30.0 | 28.7 / 29.1 | 0 / 0 | 34 | 30.0 (30.0) | OK |

**creator** (speed tour 2: `speed2_creator.txt`)

| window | logic t/s | drawn fps | work avg / max ms | >33 / >50 ms | worst ms | audit 2 fps (logic) | verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| creator: species and body tab | 30.0 | 30.0 | 15.5 / 34.1 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| creator: tab change to head and face | 30.0 | 28.5 | 20.7 / 59.0 | 2 / 1 | 67 | 28.5 (30.0) | MISS 1x |
| creator: head and face tab | 30.0 | 29.7 | 16.5 / 60.3 | 1 / 1 | 67 | 29.7 (30.0) | MISS 1x |
| creator: tab change to hair | 30.8 | 29.2 | 18.8 / 55.3 | 2 / 1 | 67 | 29.2 (30.8) | MISS 1x |
| creator: hair tab | 30.2 | 29.8 | 15.6 / 50.9 | 1 / 0 | 50 | 29.8 (30.2) | MISS 1x |
| creator: tab change to supernatural | 32.4 | 29.2 | 17.8 / 56.5 | 2 / 1 | 67 | 29.2 (32.4) | MISS 1x |
| creator: supernatural tab | 30.0 | 29.7 | 15.9 / 55.9 | 1 / 1 | 67 | 29.7 (30.0) | MISS 1x |
| creator: tab change to outfit | 31.6 | 28.4 | 26.0 / 67.4 | 2 / 1 | 67 | 29.2 (32.4) | MISS 1x |
| creator: outfit tab | 30.0 | 30.0 | 18.3 / 36.3 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| creator: tab change to voice | 32.4 | 29.2 | 20.1 / 55.6 | 2 / 1 | 67 | 29.2 (32.4) | MISS 1x |
| creator: voice tab | 30.0 | 30.0 | 15.5 / 33.6 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| creator: tab change to personality | 31.6 | 30.0 | 26.3 / 44.2 | 1 / 0 | 50 | 30.0 (31.6) | MISS 1x |
| creator: personality tab | 30.0 | 30.0 | 24.4 / 26.3 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| creator: tab change to profile | 31.6 | 30.0 | 18.6 / 51.0 | 2 / 0 | 50 | 30.0 (31.6) | MISS 1x |
| creator: profile tab | 30.0 | 30.0 | 15.5 / 28.7 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| creator: tab change to home | 31.6 | 30.0 | 27.0 / 44.7 | 1 / 0 | 50 | 30.0 (31.6) | MISS 1x |
| creator: home tab | 30.2 | 29.8 | 25.5 / 43.5 | 2 / 0 | 50 | 29.8 (30.2) | MISS 1x |
| creator: grid (first row) | 30.0 | 30.0 | 15.3 / 16.7 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| creator: grid cursor move | 30.0 | 30.0 | 24.8 / 37.3 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| creator: hat grid (bust camera) | 29.8 | 27.2 | 21.7 / 66.7 | 14 / 1 | 67 | 27.2 (29.8) | MISS hitch |
| creator: hat grid try-on move | 28.6 | 15.2 | 51.6 / 77.2 | 15 / 9 | 100 | 15.2 (28.6) | MISS slow |
| creator: hat grid filter chip | 31.0 | 21.7 | 37.8 / 49.2 | 19 / 0 | 50 | 21.7 (31.0) | MISS low |
| creator: colour swatches | 30.0 | 30.0 | 32.0 / 33.6 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| creator: swatch move | 30.0 | 26.0 | 36.5 / 99.6 | 6 / 1 | 100 | 25.2 (29.0) | MISS hitch |
| creator: start menu | 30.0 | 30.0 | 9.9 / 11.3 | 0 / 0 | 34 | 12.2 (22.9) | OK |

**life** (speed tour 2: `speed2_life.txt`)

| window | logic t/s | drawn fps | work avg / max ms | >33 / >50 ms | worst ms | audit 2 fps (logic) | verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| life: new island -> hub (the load) | 13.0 | 7.9 | 107.4 / 1996.2 | 39 / 39 | 1992 | 8.4 (13.2) | MISS slow |
| life: family dashboard | 30.0 | 30.0 | 22.4 / 24.0 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: add member: kind | 29.0 | 25.2 | 35.3 / 45.5 | 19 / 0 | 50 | 25.5 (30.0) | MISS low |
| life: add member: pick a resident | 30.0 | 30.0 | 30.8 / 32.2 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: add member: pick move | 29.5 | 25.6 | 34.7 / 95.2 | 4 / 3 | 100 | 25.4 (30.5) | MISS hitch |
| life: add member: role | 30.0 | 27.0 | 34.4 / 45.1 | 12 / 0 | 50 | 27.0 (30.0) | MISS hitch |
| life: family with a member | 30.0 | 30.0 | 26.3 / 30.3 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: play-as switcher | 30.0 | 30.0 | 15.4 / 16.0 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: play-as move | 30.0 | 30.0 | 15.4 / 15.9 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: options controls | 30.0 | 30.0 | 26.1 / 28.2 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: options page turn to shooter | 30.0 | 30.0 | 25.7 / 25.9 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: options shooter | 30.0 | 30.0 | 25.7 / 29.0 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: options page turn to life | 30.0 | 30.0 | 25.7 / 27.8 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: options life | 30.0 | 30.0 | 25.8 / 26.4 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: options page turn to audio | 30.0 | 30.0 | 24.9 / 25.6 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: options audio | 30.0 | 30.0 | 25.1 / 27.6 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: options page turn to display | 30.0 | 30.0 | 24.0 / 24.4 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: options display | 30.0 | 30.0 | 24.2 / 27.3 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: options page turn to performance | 30.0 | 30.0 | 24.1 / 24.4 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: options performance | 30.0 | 30.0 | 24.3 / 27.8 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: options page turn to accessibility | 27.9 | 23.7 | 31.8 / 47.4 | 5 / 2 | 67 | 23.7 (27.9) | MISS low |
| life: options accessibility | 30.3 | 28.2 | 28.0 / 42.5 | 9 / 0 | 50 | 28.2 (30.3) | MISS hitch |
| life: options page turn to cheats | 30.0 | 30.0 | 24.8 / 25.2 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: options cheats | 30.0 | 30.0 | 24.8 / 27.6 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: options page turn to game | 30.0 | 30.0 | 23.8 / 24.4 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: options game | 30.0 | 30.0 | 24.1 / 27.2 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: options cursor move | 30.0 | 30.0 | 24.1 / 26.6 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: save/load | 30.0 | 30.0 | 23.2 / 25.7 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: save/load cursor move | 30.0 | 30.0 | 23.4 / 26.1 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: saving (the save freeze) | 3.4 | 3.4 | 286.0 / 11425.3 | 2 / 1 | 11435 | 3.4 (3.4) | MISS slow |
| life: care popup (feed) | 30.0 | 30.0 | 12.1 / 18.6 | 0 / 0 | 34 | 14.5 (29.0) | OK |
| life: care popup, toast gone | 30.0 | 30.0 | 9.3 / 16.4 | 0 / 0 | 34 | new | OK |
| life: care reaction | 30.8 | 19.0 | 44.5 / 52.2 | 37 / 4 | 67 | 17.3 (30.5) | MISS low |
| life: shop (food) | 30.0 | 29.7 | 32.1 / 38.8 | 2 / 0 | 50 | 29.8 (30.2) | MISS 1x |
| life: shop cursor move | 30.0 | 30.0 | 33.1 / 35.8 | 0 / 0 | 34 | 29.5 (29.5) | OK |
| life: shop quantity popup | 30.5 | 30.0 | 5.5 / 9.1 | 0 / 0 | 34 | 13.3 (25.0) | OK |
| life: shop gifts tab | 30.0 | 21.5 | 35.5 / 59.7 | 17 / 14 | 100 | 30.0 (30.0) | MISS low |
| life: black market | 29.8 | 29.5 | 32.4 / 42.2 | 2 / 0 | 50 | 22.9 (29.8) | MISS 1x |
| life: cafe counter | 30.0 | 29.7 | 30.7 / 39.3 | 2 / 0 | 50 | 29.8 (29.8) | MISS 1x |
| life: news help wanted | 30.0 | 30.0 | 29.5 / 31.5 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: news third tab | 29.8 | 29.8 | 32.6 / 34.5 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: graveyard | 30.0 | 30.0 | 13.9 / 16.0 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| life: unlocks | 30.0 | 30.0 | 30.9 / 37.2 | 0 / 0 | 34 | 11.2 (19.1) | OK |
| life: unlocks cursor move | 30.0 | 30.0 | 30.1 / 34.7 | 0 / 0 | 34 | 11.3 (20.0) | OK |
| life: unlocks row moves | 30.0 | 28.9 | 32.2 / 49.9 | 6 / 0 | 50 | new | MISS hitch |
| life: unlocks paints tab | 30.0 | 30.0 | 25.3 / 27.0 | 0 / 0 | 34 | 15.0 (30.0) | OK |
| life: citizen picker | 29.8 | 22.9 | 35.0 / 57.7 | 21 / 14 | 100 | 29.7 (30.0) | MISS low |
| life: citizen picker move | 29.5 | 25.6 | 35.0 / 95.4 | 4 / 3 | 100 | 27.5 (30.5) | MISS hitch |

**story** (speed tour 2: `speed2_story.txt`)

| window | logic t/s | drawn fps | work avg / max ms | >33 / >50 ms | worst ms | audit 2 fps (logic) | verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| story: first boot language picker | 30.0 | 30.0 | 12.6 / 14.1 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| story: language picker move | 30.0 | 30.0 | 12.5 / 13.3 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| story: first boot security code | 30.0 | 30.0 | 14.3 / 16.4 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| story: security code digit | 30.0 | 30.0 | 14.2 / 14.9 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| story: title -> new story (the load) | 12.5 | 12.2 | 80.3 / 2068.9 | 6 / 1 | 2076 | 11.9 (12.5) | MISS slow |
| story: prologue cutscene | 30.2 | 30.2 | 32.1 / 33.4 | 0 / 0 | 34 | 30.2 (30.2) | OK |
| story: quest log story tab | 30.0 | 30.0 | 21.9 / 22.6 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| story: quest log side tab | 30.0 | 30.0 | 29.8 / 32.4 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| story: quest log side cursor move | 29.5 | 29.5 | 29.8 / 33.3 | 1 / 0 | 50 | 29.5 (29.5) | MISS 1x |
| story: quest log items tab | 30.0 | 30.0 | 20.6 / 21.8 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| story: quest log map tab | 30.0 | 30.0 | 27.5 / 28.2 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| story: quest log map cursor move | 30.0 | 30.0 | 27.5 / 28.0 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| story: quest log friends tab | 30.0 | 30.0 | 24.8 / 25.7 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| story: quest log trophies tab | 30.0 | 30.0 | 18.9 / 20.4 | 0 / 0 | 34 | 30.0 (30.0) | OK |
| story: cutscene chapter card 2 | 30.0 | 15.0 | 58.9 / 60.8 | 45 / 45 | 67 | 15.0 (30.0) | MISS low |
| story: cutscene chapter card 3 | 29.8 | 29.8 | 17.5 / 30.7 | 0 / 0 | 34 | 29.8 (29.8) | OK |
| story: cutscene chapter card 4 | 30.0 | 22.7 | 37.0 / 87.2 | 20 / 12 | 84 | 23.5 (30.2) | MISS low |
| story: cutscene home vignette | 30.0 | 20.0 | 43.5 / 47.1 | 60 / 0 | 50 | 20.0 (30.0) | MISS low |
| story: cutscene fog vignette | 18.4 | 10.0 | 92.8 / 100.8 | 49 / 49 | 100 | 10.0 (18.4) | MISS slow |
| story: cutscene eruption vignette | 29.8 | 16.9 | 54.3 / 63.8 | 51 / 28 | 67 | 16.7 (30.0) | MISS low |
| story: cutscene vapula vignette | 30.2 | 16.1 | 56.9 / 63.1 | 48 / 35 | 67 | 12.0 (21.7) | MISS low |
| story: cutscene after a press | 30.0 | 20.0 | 45.0 / 51.9 | 40 / 0 | 50 | 17.9 (30.8) | MISS low |
| story: credits | 30.0 | 29.3 | 30.3 / 46.7 | 6 / 0 | 50 | 7.2 (7.2) | MISS hitch |
| story: credits parade | 30.0 | 26.3 | 34.3 / 51.4 | 32 / 0 | 50 | 5.2 (5.3) | MISS low |

**tour 1** (the first speed tour: `speed_tour.txt` (`make -C gba test-speed SPEED_GATES=0`))

| window | logic t/s | drawn fps | work avg / max ms | >33 / >50 ms | worst ms | audit 2 fps (logic) | verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| title | 30.0 | 28.6 | 27.9 / 64.9 | 5 / 5 | 84 | - | MISS hitch |
| hahaland visit | 30.0 | 30.0 | 28.0 / 30.6 | 0 / 0 | 34 | - | OK |
| overworld pause | 29.9 | 29.9 | 23.8 / 39.8 | 1 / 0 | 50 | - | MISS 1x |
| hellvile visit | 30.0 | 30.0 | 21.7 / 24.1 | 0 / 0 | 34 | - | OK |
| park visit | 30.0 | 30.0 | 25.8 / 28.3 | 0 / 0 | 34 | - | OK |
| map walk | 30.0 | 30.0 | 26.9 / 32.7 | 0 / 0 | 34 | - | OK |
| invaded | 30.2 | 30.0 | 26.8 / 35.9 | 1 / 0 | 50 | - | MISS 1x |
| story map + dialogue | 30.0 | 30.0 | 8.2 / 13.0 | 0 / 0 | 34 | - | OK |
| concert | 30.1 | 16.2 | 42.7 / 46.8 | 70 / 51 | 67 | - | MISS low |
| combat first person | 30.1 | 18.3 | 44.6 / 92.1 | 79 / 19 | 100 | - | combat (ref) |
| combat third person | 29.9 | 17.9 | 41.8 / 115.7 | 78 / 19 | 134 | - | combat (ref) |
| house plain | 30.0 | 30.0 | 11.1 / 13.1 | 0 / 0 | 34 | - | OK |
| options | 30.0 | 30.0 | 25.7 / 26.7 | 0 / 0 | 34 | - | OK |
| island hub | 30.0 | 30.0 | 14.4 / 33.7 | 0 / 0 | 34 | - | OK |
| island town hall menu | 29.1 | 26.2 | 31.3 / 75.5 | 16 / 9 | 117 | - | MISS slow |
| pet | 29.7 | 28.5 | 29.0 / 60.7 | 5 / 5 | 84 | - | MISS hitch |
| exorcism tools | 30.0 | 21.5 | 39.8 / 47.9 | 69 / 5 | 67 | - | MISS low |

**life tour** (`life_speed.txt` (`make -C gba test-life`))

| window | logic t/s | drawn fps | work avg / max ms | >33 / >50 ms | worst ms | audit 2 fps (logic) | verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| life: island hub | 30.1 | 30.1 | 14.5 / 33.5 | 0 / 0 | 34 | - | OK |
| life: news | 30.0 | 29.3 | 32.2 / 36.7 | 6 / 0 | 50 | - | MISS hitch |
| life: island menu | 30.0 | 30.0 | 28.9 / 34.9 | 0 / 0 | 34 | - | OK |
| life: roster | 30.1 | 29.9 | 31.0 / 39.5 | 1 / 0 | 50 | - | MISS 1x |
| life: resident | 30.1 | 29.4 | 30.8 / 40.2 | 5 / 0 | 50 | - | MISS hitch |
| life: apartments | 30.1 | 25.7 | 32.6 / 51.6 | 18 / 15 | 84 | - | MISS hitch |

**SP4 tour** (`sp4_tour.txt` (pet, roster, resident, apartments, LYRICS))

| window | logic t/s | drawn fps | work avg / max ms | >33 / >50 ms | worst ms | audit 2 fps (logic) | verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| pet main | 30.0 | 30.0 | 28.2 / 31.9 | 0 / 0 | 34 | - | OK |
| pet cursor | 30.0 | 29.5 | 28.6 / 48.5 | 2 / 0 | 50 | - | MISS 1x |
| pet health | 30.0 | 29.7 | 30.0 / 41.8 | 2 / 0 | 50 | - | MISS 1x |
| pet dex | 30.0 | 30.0 | 22.1 / 24.3 | 0 / 0 | 34 | - | OK |
| pet boss | 29.8 | 28.6 | 28.6 / 48.2 | 7 / 5 | 67 | - | MISS hitch |
| life roster | 30.0 | 29.8 | 30.7 / 44.3 | 2 / 0 | 50 | - | MISS 1x |
| roster cursor | 29.0 | 19.8 | 42.3 / 95.1 | 20 / 15 | 117 | - | MISS slow |
| life resident | 30.0 | 29.3 | 31.1 / 39.8 | 6 / 0 | 50 | - | MISS hitch |
| life apartments | 30.0 | 30.0 | 30.7 / 33.8 | 0 / 0 | 34 | - | OK |
| lyrics | 30.0 | 30.0 | 32.2 / 33.6 | 0 / 0 | 34 | - | OK |
| lyrics after | 30.3 | 29.7 | 31.9 / 33.4 | 1 / 0 | 50 | - | MISS 1x |

**exorcism** (`exorcism_profile.txt` (`make -C gba profile-exorcism`): the ritual phase by phase)

| window | logic t/s | drawn fps | work avg / max ms | >33 / >50 ms | worst ms | audit 2 fps (logic) | verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| exorcism intro (dialogue) | 30.0 | 22.7 | 37.0 / 44.6 | 42 / 0 | 50 | - | MISS low |
| exorcism tools | 30.0 | 28.3 | 33.7 / 42.5 | 10 / 0 | 50 | - | MISS hitch |
| exorcism prayer | 29.9 | 19.2 | 45.8 / 56.1 | 77 / 8 | 84 | - | MISS low |
| exorcism holy water | 29.7 | 23.4 | 36.6 / 48.5 | 44 / 4 | 67 | - | MISS low |
| exorcism true name | 30.0 | 19.5 | 42.5 / 47.0 | 39 / 3 | 67 | - | MISS low |
| exorcism verdict | 31.0 | 19.7 | 45.1 / 53.5 | 38 / 3 | 67 | - | MISS low |
| exorcism result | 30.2 | 19.1 | 43.2 / 47.7 | 57 / 9 | 67 | - | MISS low |

**concert** (`concert_profile.txt` (`make -C gba profile-concert`))

| window | logic t/s | drawn fps | work avg / max ms | >33 / >50 ms | worst ms | audit 2 fps (logic) | verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| concert solo | 30.0 | 22.2 | 37.0 / 43.2 | 68 / 0 | 50 | - | MISS low |
| concert band | 23.6 | 13.2 | 56.7 / 62.9 | 67 / 61 | 100 | - | MISS slow |
| concert choir | 30.0 | 15.0 | 46.9 / 50.5 | 65 / 65 | 67 | - | MISS low |
| concert setup | 28.3 | 14.5 | 60.1 / 68.3 | 46 / 46 | 84 | - | MISS slow |

**creator (held input)** (`creator_profile.txt` and `creator_scroll.txt` (`profile-creator`, `test-creator`))

| window | logic t/s | drawn fps | work avg / max ms | >33 / >50 ms | worst ms | audit 2 fps (logic) | verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| creator idle | 30.0 | 30.0 | 15.5 / 34.1 | 0 / 0 | 34 | - | OK |
| creator value step | 29.9 | 23.2 | 36.8 / 79.2 | 45 / 11 | 84 | - | MISS low |
| creator row scroll | 30.0 | 27.2 | 26.0 / 50.0 | 23 / 0 | 50 | - | MISS hitch |
| creator picker | 29.8 | 21.8 | 39.0 / 55.9 | 60 / 10 | 67 | - | MISS low |
| creator idle (scroll script) | 30.0 | 30.0 | 15.5 / 34.1 | 0 / 0 | 34 | - | OK |
| creator grid idle (scroll script) | 30.0 | 30.0 | 15.9 / 30.4 | 0 / 0 | 34 | - | OK |
| creator grid scroll (scroll script) | 29.8 | 17.2 | 45.0 / 73.7 | 53 / 40 | 100 | - | MISS low |
| creator rows (scroll script) | 29.9 | 29.4 | 23.2 / 38.1 | 4 / 0 | 50 | - | MISS hitch |
| creator value step (scroll script) | 29.8 | 22.2 | 37.1 / 66.8 | 55 / 10 | 84 | - | MISS low |

**studio tabs and PERFORM** (`studio_profile.txt` and `studio_sung.txt` (`profile-studio`, `profile-sung`))

| window | logic t/s | drawn fps | work avg / max ms | >33 / >50 ms | worst ms | audit 2 fps (logic) | verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| song idle | 30.0 | 30.0 | 31.3 / 32.9 | 0 / 0 | 34 | - | OK |
| song playing | 30.0 | 30.0 | 32.0 / 36.7 | 0 / 0 | 34 | - | OK |
| notes idle | 30.0 | 30.0 | 27.5 / 28.8 | 0 / 0 | 34 | - | OK |
| notes playing | 30.0 | 30.0 | 27.7 / 31.6 | 0 / 0 | 34 | - | OK |
| order | 30.0 | 30.0 | 25.3 / 26.6 | 0 / 0 | 34 | - | OK |
| lyrics | 30.0 | 30.0 | 32.1 / 34.0 | 0 / 0 | 34 | - | OK |
| choir | 30.0 | 30.0 | 30.8 / 32.0 | 1 / 0 | 50 | - | MISS 1x |
| perform early | 30.0 | 30.0 | 20.3 / 20.5 | 0 / 0 | 34 | - | OK |
| perform later | 30.0 | 30.0 | 21.2 / 30.2 | 0 / 0 | 34 | - | OK |
| PERFORM, sung (330 ticks) | 30.0 | 30.0 | 23.7 / 30.1 | 0 / 0 | 34 | - | OK |

**title, pause, invaded** (`title_profile.txt`, `pause_profile.txt`, `invaded_profile.txt`)

| window | logic t/s | drawn fps | work avg / max ms | >33 / >50 ms | worst ms | audit 2 fps (logic) | verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| title idle | 30.0 | 28.6 | 28.0 / 64.9 | 5 / 5 | 84 | - | MISS hitch |
| title menu open | 30.0 | 26.8 | 32.2 / 82.3 | 16 / 5 | 100 | - | MISS hitch |
| visit | 30.0 | 30.0 | 28.0 / 30.6 | 0 / 0 | 34 | - | OK |
| pause | 30.0 | 29.5 | 23.9 / 43.4 | 4 / 0 | 50 | - | MISS hitch |
| invaded | 30.0 | 30.0 | 28.8 / 35.9 | 0 / 0 | 34 | - | OK |

**tip cards** (`tips.txt` (`make -C gba test-tips`))

| window | logic t/s | drawn fps | work avg / max ms | >33 / >50 ms | worst ms | audit 2 fps (logic) | verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| island, tip card up | 23.8 | 19.0 | 34.7 / 88.7 | 25 / 21 | 117 | - | MISS slow |
| island, card closed | 30.2 | 30.2 | 14.3 / 31.0 | 0 / 0 | 34 | - | OK |
| field guide page | 30.0 | 30.0 | 9.1 / 9.4 | 0 / 0 | 34 | - | OK |

**story chapters** (`story_ch1.txt`, `story_ch4.txt` (`make -C gba test-story`))

| window | logic t/s | drawn fps | work avg / max ms | >33 / >50 ms | worst ms | audit 2 fps (logic) | verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| P0 potluck | 30.0 | 30.0 | 32.1 / 33.2 | 0 / 0 | 34 | - | OK |
| P0 bridge | 29.8 | 20.3 | 40.3 / 41.0 | 62 / 0 | 50 | - | MISS low |
| P0 fireworks | 30.0 | 29.5 | 30.9 / 40.2 | 1 / 0 | 50 | - | MISS 1x |
| overworld | 26.5 | 24.2 | 25.5 / 241.6 | 24 / 19 | 251 | - | MISS slow |
| CUTV_FOG | 30.0 | 30.0 | 27.6 / 29.5 | 0 / 0 | 34 | - | OK |
| CUTV_GATE_CRACKS | 30.3 | 28.2 | 33.1 / 37.3 | 6 / 0 | 50 | - | MISS hitch |
| summit | 30.0 | 30.0 | 16.4 / 19.4 | 0 / 0 | 34 | - | OK |
| cutscene_wall | 30.2 | 29.8 | 18.4 / 40.0 | 1 / 0 | 50 | - | MISS 1x |
| park | 29.3 | 28.0 | 33.8 / 58.2 | 10 / 1 | 84 | - | MISS slow |

**house test layouts** (`home.txt` (`make -C gba test-home`; `room-test` stress layouts, not a player's house))

| window | logic t/s | drawn fps | work avg / max ms | >33 / >50 ms | worst ms | audit 2 fps (logic) | verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| home_plain | 30.0 | 30.0 | 11.1 / 13.1 | 0 / 0 | 34 | - | OK |
| home_steady | 29.8 | 29.2 | 29.8 / 41.9 | 5 / 0 | 50 | - | MISS hitch |
| home_modal | 30.0 | 30.0 | 13.1 / 13.5 | 0 / 0 | 34 | - | OK |
| home_modal_busy | 25.7 | 22.7 | 26.6 / 159.9 | 9 / 9 | 167 | - | MISS slow |
| home_rotate | 29.8 | 24.5 | 36.5 / 134.9 | 16 / 2 | 134 | - | MISS low |
| home_cellar_full | 29.8 | 26.2 | 25.7 / 51.4 | 23 / 0 | 50 | - | MISS low |
| home_defend_w3 | 23.1 | 12.6 | 73.5 / 84.6 | 49 / 49 | 84 | - | MISS slow |

**combat detail (reference)** (`view3d.txt` (`make -C gba test-3d`))

| window | logic t/s | drawn fps | work avg / max ms | >33 / >50 ms | worst ms | audit 2 fps (logic) | verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| smooth first person | 29.9 | 18.2 | 43.1 / 91.5 | 79 / 20 | 100 | - | combat (ref) |
| smooth third person | 29.9 | 18.2 | 39.2 / 78.5 | 79 / 21 | 84 | - | combat (ref) |
| balanced first person | 30.1 | 18.5 | 42.8 / 78.8 | 80 / 20 | 84 | - | combat (ref) |
| balanced third person | 29.9 | 17.9 | 39.7 / 69.2 | 78 / 24 | 84 | - | combat (ref) |
| max detail first person | 30.0 | 18.7 | 42.7 / 75.7 | 81 / 18 | 84 | - | combat (ref) |

### 2.2 PS1 (PCSX-Redux)

Tags are the GBA window names in lower case. One tick a frame: fps is also the game speed, so a window under
29.5 fps is "slow". "Overlay loads" are the `ovl:` reads since the previous window (the PS1's scene-change
freeze). A lone 3-vsync frame is the lockstep resume and is not counted; with two, one is real.

**fair** (speed tour 2: `speed2_fair.txt`)

| window | fps (= logic) | frames by vsyncs | >33 / >50 ms | worst ms | overlay loads ms | audit 2 fps | verdict |
|---|---:|---|---:|---:|---|---:|---|
| fair_catch_title | 30.17 | 2:89 | 0 / 0 | 33 | 314+314 | 30.17 | OK |
| fair_catch_play_sweet | 30.12 | 2:129 | 0 / 0 | 33 | 314+313 | 30.12 | OK |
| fair_catch_results | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| fair_catch_play_hellfire | 30.12 | 2:129 | 0 / 0 | 33 | 314+314 | 30.12 | OK |
| fair_whack_title | 30.17 | 2:89 | 0 / 0 | 33 | 311+311 | 30.17 | OK |
| fair_whack_play_sweet | 30.12 | 2:129 | 0 / 0 | 33 | 315+314 | 30.12 | OK |
| fair_whack_play_hellfire | 30.12 | 2:129 | 0 / 0 | 33 | 314+316 | 30.12 | OK |
| fair_dodge_title | 30.17 | 2:89 | 0 / 0 | 33 | 314+313 | 30.17 | OK |
| fair_dodge_play_sweet | 30.12 | 2:129 | 0 / 0 | 33 | 313+311 | 30.12 | OK |
| fair_dodge_play_late | 30.12 | 2:129 | 0 / 0 | 33 | - | 30.12 | OK |
| fair_dodge_play_hellfire | 30.12 | 2:129 | 0 / 0 | 33 | 313+316 | 30.12 | OK |
| fair_gallery_title | 30.17 | 2:89 | 0 / 0 | 33 | 313+316 | 30.17 | OK |
| fair_gallery_play_sweet | 30.00 | 2:128 3:1 | 0 / 0 | 50 | 312+315 | 30.00 | OK |
| fair_gallery_play_hellfire | 30.00 | 2:128 3:1 | 0 / 0 | 50 | 315+315 | 30.00 | OK |
| fair_derby_title | 30.17 | 2:89 | 0 / 0 | 33 | 311+312 | 30.17 | OK |
| fair_derby_race_sweet | 30.00 | 2:128 3:1 | 0 / 0 | 50 | 314+313 | 30.00 | OK |
| fair_derby_race_mid | 30.00 | 2:128 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| fair_derby_race_hellfire | 30.00 | 2:128 3:1 | 0 / 0 | 50 | 314+312 | 30.00 | OK |
| fair_memory_title | 30.17 | 2:89 | 0 / 0 | 33 | 316+312 | 30.17 | OK |
| fair_memory_play_sweet | 30.12 | 2:129 | 0 / 0 | 33 | 312+314 | 30.12 | OK |
| fair_memory_play_hellfire | 30.12 | 2:129 | 0 / 0 | 33 | 315+312 | 30.12 | OK |
| fair_wheel_intro | 30.00 | 2:88 3:1 | 0 / 0 | 50 | 314+313 | 22.41 | OK |
| fair_wheel_spin | 30.00 | 2:128 3:1 | 0 / 0 | 50 | - | 22.41 | OK |

**fishcook** (speed tour 2: `speed2_fishcook.txt`)

| window | fps (= logic) | frames by vsyncs | >33 / >50 ms | worst ms | overlay loads ms | audit 2 fps | verdict |
|---|---:|---|---:|---:|---|---:|---|
| fish_pier_day | 30.12 | 2:129 | 0 / 0 | 33 | 314+286 | 30.12 | OK |
| fish_bot_cast_and_wait | 30.12 | 2:129 | 0 / 0 | 33 | - | 30.12 | OK |
| fish_reel | 30.51 | 2:29 | 0 / 0 | 33 | - | 30.51 | OK |
| fish_catch_card | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| fish_fishdex | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| fish_cooler | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| fish_night_reel | 30.15 | 2:99 | 0 / 0 | 33 | - | 30.15 | OK |
| cook_recipe_book | 30.17 | 2:89 | 0 / 0 | 33 | 313+288 | 30.17 | OK |
| cook_chop | 30.12 | 2:129 | 0 / 0 | 33 | - | 30.12 | OK |
| cook_stir | 30.12 | 2:129 | 0 / 0 | 33 | - | 30.12 | OK |
| cook_fry | 30.12 | 2:129 | 0 / 0 | 33 | - | 30.12 | OK |
| cook_result_card | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| cook_bake | 30.12 | 2:129 | 0 / 0 | 33 | - | 30.12 | OK |
| cook_simmer | 30.12 | 2:129 | 0 / 0 | 33 | - | 30.12 | OK |
| cook_counter | 29.83 | 2:87 3:2 | 2 / 0 | 50 | - | 29.83 | MISS 1x |
| cook_serving | 30.00 | 2:58 3:1 | 0 / 0 | 50 | - | 30.00 | OK |

**library** (speed tour 2: `speed2_library.txt`)

| window | fps (= logic) | frames by vsyncs | >33 / >50 ms | worst ms | overlay loads ms | audit 2 fps | verdict |
|---|---:|---|---:|---:|---|---:|---|
| library_lobby | 30.12 | 2:129 | 0 / 0 | 33 | 288 | 30.12 | OK |
| library_lobby_shelf_move | 29.51 | 2:28 4:1 | 1 / 1 | 67 | - | 29.51 | MISS 1x |
| library_bestiary_list | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| library_bestiary_jar | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| library_almanac_list | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| library_almanac_list_move | 29.51 | 2:28 4:1 | 1 / 1 | 67 | - | 29.51 | MISS 1x |
| library_almanac_profile | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| library_almanac_face | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| library_weapons_frames | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| library_weapons_plan | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| library_items | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| library_items_card | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| library_trophies | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| library_trophy_page | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| library_jukebox | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| library_jukebox_playing | 30.00 | 2:128 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| library_gallery_textures | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| library_gallery_tiled | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| library_gallery_sky | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| library_field_guide_contents | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| library_field_guide_page | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| library_field_guide_page_turn | 30.77 | 2:19 | 0 / 0 | 33 | - | 30.77 | OK |
| library_field_guide_scroll | 30.77 | 2:19 | 0 / 0 | 33 | - | 30.77 | OK |

**action** (speed tour 2: `speed2_action.txt`)

| window | fps (= logic) | frames by vsyncs | >33 / >50 ms | worst ms | overlay loads ms | audit 2 fps | verdict |
|---|---:|---|---:|---:|---|---:|---|
| action_level_select | 30.00 | 2:128 3:1 | 0 / 0 | 50 | 314+361 | 30.00 | OK |
| action_level_select_move | 30.77 | 2:19 | 0 / 0 | 33 | - | 30.77 | OK |
| action_breach_intro_card | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | 22.88 | OK |
| action_breach_play_combat_reference | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | 30.00 | combat (ref) |
| action_breach_pause_menu | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| action_level_select_after_a_level | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| action_arena_intro_card | 30.00 | 2:88 3:1 | 0 / 0 | 50 | 314+359 | 30.00 | OK |
| action_arena_fight_combat_reference | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | 30.00 | combat (ref) |
| action_arena_pause_menu | 30.00 | 2:58 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| action_arena_results | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| action_boss_rush_intro_card | 30.00 | 2:88 3:1 | 0 / 0 | 50 | 316+361 | 30.00 | OK |
| action_boss_rush_round_1_combat_reference | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | 30.00 | combat (ref) |
| action_boss_rush_results | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| action_horde_intro_card | 30.00 | 2:88 3:1 | 0 / 0 | 50 | 314+362 | 30.00 | OK |
| action_horde_results | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| action_mayhem_intro_card | 30.00 | 2:88 3:1 | 0 / 0 | 50 | 314+362 | 30.00 | OK |
| action_mayhem_chaos_combat_reference | 29.83 | 2:87 3:2 | 2 / 0 | 50 | - | 29.51 | combat (ref) |
| action_workshop | 28.78 | 2:126 6:3 | 3 / 3 | 100 | 315+125 | 28.78 | MISS slow |
| action_workshop_part_change | 30.51 | 2:29 | 0 / 0 | 33 | - | 30.51 | OK |
| action_workshop_armory | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| action_weld_intro_card | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| action_welding | 30.12 | 2:129 | 0 / 0 | 33 | - | 30.00 | OK |

**dream** (speed tour 2: `speed2_dream.txt`)

| window | fps (= logic) | frames by vsyncs | >33 / >50 ms | worst ms | overlay loads ms | audit 2 fps | verdict |
|---|---:|---|---:|---:|---|---:|---|
| dream_bedroom | 30.17 | 2:89 | 0 / 0 | 33 | 182 | 30.17 | OK |
| dream_thought_bubble | 30.25 | 2:59 | 0 / 0 | 33 | - | 30.25 | OK |
| dream_vignette_1_cute | 30.00 | 2:108 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| dream_vignette_1_twist | 30.00 | 2:98 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| dream_vignette_2 | 30.00 | 2:128 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| dream_nightmare_vignette_1 | 30.00 | 2:128 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| dream_nightmare_vignette_2 | 29.89 | 2:128 4:1 | 1 / 1 | 67 | - | 29.89 | MISS 1x |
| dream_nightmare_vignette_3_cute_then_red | 30.00 | 2:98 3:1 | 0 / 0 | 50 | - | 29.85 | OK |
| dream_wake_up_choice | 30.00 | 1:1 2:57 4:1 | 1 / 1 | 67 | - | 29.75 | MISS 1x |

**house** (speed tour 2: `speed2_house.txt`)

| window | fps (= logic) | frames by vsyncs | >33 / >50 ms | worst ms | overlay loads ms | audit 2 fps | verdict |
|---|---:|---|---:|---:|---|---:|---|
| house_build_mode | 30.12 | 2:129 | 0 / 0 | 33 | 502 | 30.12 | OK |
| house_build_cursor_move | 30.51 | 2:29 | 0 / 0 | 33 | - | 30.51 | OK |
| house_catalog_modal | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| house_catalog_cursor_move | 30.51 | 2:29 | 0 / 0 | 33 | - | 30.51 | OK |
| house_held_piece | 30.25 | 2:59 | 0 / 0 | 33 | - | 30.25 | OK |
| house_rotate_view | 30.51 | 2:29 | 0 / 0 | 33 | - | 30.51 | OK |
| house_photo_mode | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| house_photo_snap | 30.51 | 2:29 | 0 / 0 | 33 | - | 30.51 | OK |
| house_live_mode | 30.12 | 2:129 | 0 / 0 | 33 | - | 30.12 | OK |
| house_score_panel | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| house_live_4_chibis_room_test_1 | 28.89 | 2:120 3:8 4:1 | 9 / 1 | 67 | 98 | 28.89 | MISS slow |
| house_defend_tutorial | 30.12 | 2:129 | 0 / 0 | 33 | 314+502+100 | 30.12 | OK |
| house_defend_siege_wave_3 | 30.12 | 2:129 | 0 / 0 | 33 | 314+496+99 | 30.12 | OK |
| house_defend_siege_freeze_double_clock | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| house_visit_nana | 30.12 | 2:129 | 0 / 0 | 33 | 316+501+106 | 30.12 | OK |
| house_homemart | 30.17 | 2:89 | 0 / 0 | 33 | 316+500+114 | 30.17 | OK |
| house_homemart_cursor_move | 30.51 | 2:29 | 0 / 0 | 33 | - | 30.51 | OK |
| house_homemart_season_tab | 30.25 | 2:59 | 0 / 0 | 33 | - | 30.25 | OK |
| house_black_market | 30.17 | 2:89 | 0 / 0 | 33 | 313+501+113 | 30.17 | OK |

**studio** (speed tour 2: `speed2_studio.txt`)

| window | fps (= logic) | frames by vsyncs | >33 / >50 ms | worst ms | overlay loads ms | audit 2 fps | verdict |
|---|---:|---|---:|---:|---|---:|---|
| studio_choir_quick_fill | 30.25 | 2:59 | 0 / 0 | 33 | 314+276 | 30.25 | OK |
| studio_choir_member_card | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| studio_citizen_picker | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| studio_citizen_picker_move | 30.00 | 2:28 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| studio_stage_tab | 30.12 | 2:129 | 0 / 0 | 33 | - | 30.12 | OK |
| studio_file_tab | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| studio_song_book | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| studio_templates | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| studio_templates_move | 30.51 | 2:29 | 0 / 0 | 33 | - | 30.51 | OK |

**creator** (speed tour 2: `speed2_creator.txt`)

| window | fps (= logic) | frames by vsyncs | >33 / >50 ms | worst ms | overlay loads ms | audit 2 fps | verdict |
|---|---:|---|---:|---:|---|---:|---|
| creator_species_and_body_tab | 30.17 | 2:89 | 0 / 0 | 33 | 314+415 | 30.17 | OK |
| creator_tab_change_to_head_and_face | 30.77 | 2:19 | 0 / 0 | 33 | - | 30.77 | OK |
| creator_head_and_face_tab | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| creator_tab_change_to_hair | 30.00 | 2:18 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| creator_hair_tab | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| creator_tab_change_to_supernatural | 30.00 | 2:18 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| creator_supernatural_tab | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| creator_tab_change_to_outfit | 30.00 | 2:18 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| creator_outfit_tab | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| creator_tab_change_to_voice | 30.77 | 2:19 | 0 / 0 | 33 | - | 30.77 | OK |
| creator_voice_tab | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| creator_tab_change_to_personality | 30.00 | 2:18 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| creator_personality_tab | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| creator_tab_change_to_profile | 30.77 | 2:19 | 0 / 0 | 33 | - | 30.77 | OK |
| creator_profile_tab | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| creator_tab_change_to_home | 30.77 | 2:19 | 0 / 0 | 33 | - | 30.77 | OK |
| creator_home_tab | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| creator_grid_first_row | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| creator_grid_cursor_move | 30.00 | 2:28 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| creator_hat_grid_bust_camera | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| creator_hat_grid_try_on_move | 28.57 | 2:26 3:2 4:1 | 3 / 1 | 67 | - | 28.57 | MISS slow |
| creator_hat_grid_filter_chip | 30.00 | 2:28 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| creator_colour_swatches | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| creator_swatch_move | 30.00 | 2:28 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| creator_start_menu | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | 30.00 | OK |

**life** (speed tour 2: `speed2_life.txt`)

| window | fps (= logic) | frames by vsyncs | >33 / >50 ms | worst ms | overlay loads ms | audit 2 fps | verdict |
|---|---:|---|---:|---:|---|---:|---|
| life_new_island_hub_the_load | 16.52 | 2:64 3:2 112:1 | 3 / 1 | 1867 | 314+207+314+209 | 14.52 | MISS slow |
| life_family_dashboard | 30.17 | 2:89 | 0 / 0 | 33 | 313+208+315 | 30.17 | OK |
| life_add_member_kind | 30.25 | 2:59 | 0 / 0 | 33 | - | 30.25 | OK |
| life_add_member_pick_a_resident | 30.00 | 2:58 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| life_add_member_pick_move | 30.51 | 2:29 | 0 / 0 | 33 | - | 30.51 | OK |
| life_add_member_role | 30.25 | 2:59 | 0 / 0 | 33 | - | 30.25 | OK |
| life_family_with_a_member | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| life_play_as_switcher | 30.17 | 2:89 | 0 / 0 | 33 | 206+310 | 30.17 | OK |
| life_play_as_move | 30.51 | 2:29 | 0 / 0 | 33 | - | 30.51 | OK |
| life_options_controls | 30.25 | 2:59 | 0 / 0 | 33 | 207+315 | 30.25 | OK |
| life_options_page_turn_to_shooter | 30.77 | 2:19 | 0 / 0 | 33 | - | 30.77 | OK |
| life_options_shooter | 30.25 | 2:59 | 0 / 0 | 33 | - | 30.25 | OK |
| life_options_page_turn_to_life | 30.77 | 2:19 | 0 / 0 | 33 | - | 30.77 | OK |
| life_options_life | 30.25 | 2:59 | 0 / 0 | 33 | - | 30.25 | OK |
| life_options_page_turn_to_audio | 30.77 | 2:19 | 0 / 0 | 33 | - | 30.77 | OK |
| life_options_audio | 30.25 | 2:59 | 0 / 0 | 33 | - | 30.25 | OK |
| life_options_page_turn_to_display | 30.77 | 2:19 | 0 / 0 | 33 | - | 30.77 | OK |
| life_options_display | 30.25 | 2:59 | 0 / 0 | 33 | - | 30.25 | OK |
| life_options_page_turn_to_performance | 30.77 | 2:19 | 0 / 0 | 33 | - | 30.77 | OK |
| life_options_performance | 30.25 | 2:59 | 0 / 0 | 33 | - | 30.25 | OK |
| life_options_page_turn_to_accessibility | 30.77 | 2:19 | 0 / 0 | 33 | - | 30.77 | OK |
| life_options_accessibility | 30.25 | 2:59 | 0 / 0 | 33 | - | 30.25 | OK |
| life_options_page_turn_to_cheats | 30.77 | 2:19 | 0 / 0 | 33 | - | 30.77 | OK |
| life_options_cheats | 30.25 | 2:59 | 0 / 0 | 33 | - | 30.25 | OK |
| life_options_page_turn_to_game | 30.77 | 2:19 | 0 / 0 | 33 | - | 30.77 | OK |
| life_options_game | 30.25 | 2:59 | 0 / 0 | 33 | - | 30.25 | OK |
| life_options_cursor_move | 30.51 | 2:29 | 0 / 0 | 33 | - | 30.51 | OK |
| life_save_load | 30.17 | 2:89 | 0 / 0 | 33 | 206+314 | 30.17 | OK |
| life_save_load_cursor_move | 30.51 | 2:29 | 0 / 0 | 33 | - | 30.51 | OK |
| life_saving_the_save_freeze | 3.25 | 1:2 2:38 | 1 / 1 | 11600 | - | 3.25 | MISS slow |
| life_care_popup_feed | 30.25 | 2:59 | 0 / 0 | 33 | 206+313 | 30.25 | OK |
| life_care_popup_toast_gone | 30.00 | 2:58 3:1 | 0 / 0 | 50 | - | new | OK |
| life_care_reaction | 29.75 | 2:57 3:2 | 2 / 0 | 50 | - | 30.00 | MISS 1x |
| life_shop_food | 30.17 | 2:89 | 0 / 0 | 33 | 206+313 | 30.00 | OK |
| life_shop_cursor_move | 30.51 | 2:29 | 0 / 0 | 33 | - | 30.51 | OK |
| life_shop_quantity_popup | 30.25 | 2:59 | 0 / 0 | 33 | - | 30.00 | OK |
| life_shop_gifts_tab | 30.25 | 2:59 | 0 / 0 | 33 | - | 30.25 | OK |
| life_black_market | 30.00 | 2:88 3:1 | 0 / 0 | 50 | 204+312 | 30.17 | OK |
| life_cafe_counter | 30.17 | 2:89 | 0 / 0 | 33 | 206+313 | 30.17 | OK |
| life_news_help_wanted | 30.17 | 2:89 | 0 / 0 | 33 | 207+312 | 30.17 | OK |
| life_news_third_tab | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| life_graveyard | 30.17 | 2:89 | 0 / 0 | 33 | 207+316 | 30.17 | OK |
| life_unlocks | 30.00 | 2:88 3:1 | 0 / 0 | 50 | 205+313 | 30.00 | OK |
| life_unlocks_cursor_move | 30.00 | 2:28 3:1 | 0 / 0 | 50 | - | 30.00 | OK |
| life_unlocks_row_moves | 30.00 | 2:70 3:1 | 0 / 0 | 50 | - | new | OK |
| life_unlocks_paints_tab | 30.25 | 2:59 | 0 / 0 | 33 | - | 30.25 | OK |
| life_citizen_picker | 30.17 | 2:89 | 0 / 0 | 33 | 206+314 | 30.00 | OK |
| life_citizen_picker_move | 30.51 | 2:29 | 0 / 0 | 33 | - | 30.51 | OK |

**story** (speed tour 2: `speed2_story.txt`)

| window | fps (= logic) | frames by vsyncs | >33 / >50 ms | worst ms | overlay loads ms | audit 2 fps | verdict |
|---|---:|---|---:|---:|---|---:|---|
| story_first_boot_language_picker | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| story_language_picker_move | 30.51 | 2:29 | 0 / 0 | 33 | - | 30.51 | OK |
| story_first_boot_security_code | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| story_security_code_digit | 30.51 | 2:29 | 0 / 0 | 33 | - | 30.51 | OK |
| story_title_new_story_the_load | 8.54 | 2:39 71:1 145:1 | 2 / 2 | 2417 | 314+369+315+367+350+366+348 | 8.78 | MISS slow |
| story_prologue_cutscene | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| story_quest_log_story_tab | 30.17 | 2:89 | 0 / 0 | 33 | 366 | 30.17 | OK |
| story_quest_log_side_tab | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| story_quest_log_side_cursor_move | 30.51 | 2:29 | 0 / 0 | 33 | - | 30.51 | OK |
| story_quest_log_items_tab | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| story_quest_log_map_tab | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| story_quest_log_map_cursor_move | 30.51 | 2:29 | 0 / 0 | 33 | - | 30.51 | OK |
| story_quest_log_friends_tab | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| story_quest_log_trophies_tab | 30.17 | 2:89 | 0 / 0 | 33 | - | 30.17 | OK |
| story_cutscene_chapter_card_2 | 30.00 | 2:88 3:1 | 0 / 0 | 50 | 351 | 26.73 | OK |
| story_cutscene_chapter_card_3 | 30.17 | 2:89 | 0 / 0 | 33 | 371+349 | 30.17 | OK |
| story_cutscene_chapter_card_4 | 30.17 | 2:89 | 0 / 0 | 33 | 369+347 | 30.17 | OK |
| story_cutscene_home_vignette | 30.17 | 2:89 | 0 / 0 | 33 | 366+347 | 30.17 | OK |
| story_cutscene_fog_vignette | 30.17 | 2:89 | 0 / 0 | 33 | 366+348 | 30.17 | OK |
| story_cutscene_eruption_vignette | 30.17 | 2:89 | 0 / 0 | 33 | 367+347 | 30.00 | OK |
| story_cutscene_vapula_vignette | 30.17 | 2:89 | 0 / 0 | 33 | 368+347 | 30.17 | OK |
| story_cutscene_after_a_press | 30.25 | 2:59 | 0 / 0 | 33 | - | 30.25 | OK |
| story_credits | 29.89 | 2:128 4:1 | 1 / 1 | 67 | 367 | 10.89 | MISS 1x |
| story_credits_parade | 30.00 | 2:128 3:1 | 0 / 0 | 50 | - | 1.52 | OK |

**perf scripts** (`ps1/harness/scripts/perf_*.txt` and `creator_speed.txt`, one at a time)

| window | fps (= logic) | frames by vsyncs | >33 / >50 ms | worst ms | overlay loads ms | audit 2 fps | verdict |
|---|---:|---|---:|---:|---|---:|---|
| title | 30.13 | 2:119 | 0 / 0 | 33 | 314 | - | OK |
| island | 29.88 | 2:117 3:2 | 2 / 0 | 50 | 314+207+314+209 | - | MISS 1x |
| townhall | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | - | OK |
| life_hub | 29.83 | 2:87 3:2 | 2 / 0 | 50 | 314+207+314+209 | - | MISS 1x |
| life_news | 30.17 | 2:89 | 0 / 0 | 33 | 313+208+315 | - | OK |
| life_menu | 30.00 | 2:88 3:1 | 0 / 0 | 50 | 206 | - | OK |
| life_roster | 30.17 | 2:89 | 0 / 0 | 33 | 310+207+315 | - | OK |
| life_resident | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | - | OK |
| life_apartments | 30.17 | 2:89 | 0 / 0 | 33 | 206+314+206+313 | - | OK |
| dialogue | 29.67 | 2:86 3:3 | 3 / 0 | 50 | 365+348+366+349+368+348+367+348+369+350+370+347+369 | - | MISS hitch |
| concert | 30.00 | 2:118 3:1 | 0 / 0 | 50 | 268 | - | OK |
| creator | 30.13 | 2:119 | 0 / 0 | 33 | 416 | - | OK |
| creator_idle | 30.13 | 2:119 | 0 / 0 | 33 | 416 | - | OK |
| creator_grid_scroll | 29.39 | 2:113 3:6 | 6 / 0 | 50 | - | - | MISS slow |
| creator_rows | 30.13 | 2:119 | 0 / 0 | 33 | - | - | OK |
| creator_value | 29.88 | 1:1 2:115 3:3 | 3 / 0 | 50 | - | - | MISS hitch |
| pet | 30.00 | 2:118 3:1 | 0 / 0 | 50 | 148 | - | OK |
| exorcism | 30.00 | 2:118 3:1 | 0 / 0 | 50 | 193 | - | OK |
| exo_intro | 30.00 | 2:88 3:1 | 0 / 0 | 50 | 314+195 | - | OK |
| exo_tools | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | - | OK |
| exo_prayer | 30.00 | 2:118 3:1 | 0 / 0 | 50 | - | - | OK |
| exo_water | 30.00 | 2:102 3:1 | 0 / 0 | 50 | - | - | OK |
| exo_name | 30.00 | 2:58 3:1 | 0 / 0 | 50 | - | - | OK |
| exo_verdict | 30.00 | 2:58 3:1 | 0 / 0 | 50 | - | - | OK |
| exo_result | 30.00 | 2:88 3:1 | 0 / 0 | 50 | - | - | OK |
| balanced_tp | 29.81 | 2:235 3:4 | 4 / 0 | 50 | 314+361 | - | combat (ref) |
| balanced_fp | 30.00 | 2:238 3:1 | 0 / 0 | 50 | 314+359 | - | combat (ref) |
| smooth_tp | 29.94 | 2:237 3:2 | 2 / 0 | 50 | 316+361 | - | combat (ref) |
| max_tp | 29.45 | 2:229 3:10 | 10 / 0 | 50 | 314+362 | - | combat (ref) |

### 2.3 Dreamcast (Flycast)

Per window: logic ticks/s and the ticks whose work was over 33.3 ms (dcrun's `work:` lines). The group line
gives dcrun's drawn count and worst tick. The first-person 3D view (DC6) is not built on the Dreamcast yet, so
the four combat windows of `action` run without it: they are listed, not counted, and say nothing about combat
speed there.

**fair** (`speed2_fair.txt`; game drew 6040 of 6040 ticks, work mean 7644 us max 15916 us; 2D prims max 2062, vertex bytes max 172224, dropped 0, texture uploads 773, evictions 0, cached 773; work over 33333 us on 0 of 6040 ticks; worst 15916 us at tick 4866 (MG_DERBY); dcrun: 0 failures)

| window | logic t/s | ticks >33.3 / >50 ms | worst ms | audit 2 | verdict |
|---|---:|---:|---:|---|---|
| fair: catch title | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fair: catch play sweet | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fair: catch results | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fair: catch play hellfire | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fair: whack title | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fair: whack play sweet | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fair: whack play hellfire | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fair: dodge title | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fair: dodge play sweet | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fair: dodge play late | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fair: dodge play hellfire | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fair: gallery title | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fair: gallery play sweet | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fair: gallery play hellfire | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fair: derby title | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fair: derby race sweet | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fair: derby race mid | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fair: derby race hellfire | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fair: memory title | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fair: memory play sweet | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fair: memory play hellfire | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fair: wheel intro | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fair: wheel spin | 30.0 | 0 / 0 | <33.3 | ok | OK |

**fishcook** (`speed2_fishcook.txt`; game drew 3335 of 3335 ticks, work mean 6514 us max 18053 us; 2D prims max 1726, vertex bytes max 128448, dropped 0, texture uploads 624, evictions 0, cached 624; work over 33333 us on 0 of 3335 ticks; worst 18053 us at tick 3263 (COOKING); dcrun: 0 failures)

| window | logic t/s | ticks >33.3 / >50 ms | worst ms | audit 2 | verdict |
|---|---:|---:|---:|---|---|
| fish: pier day | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fish: bot cast and wait | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fish: reel | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fish: catch card | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fish: fishdex | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fish: cooler | 30.0 | 0 / 0 | <33.3 | ok | OK |
| fish: night reel | 30.0 | 0 / 0 | <33.3 | ok | OK |
| cook: recipe book | 30.0 | 0 / 0 | <33.3 | ok | OK |
| cook: chop | 30.0 | 0 / 0 | <33.3 | ok | OK |
| cook: stir | 30.0 | 0 / 0 | <33.3 | ok | OK |
| cook: fry | 30.0 | 0 / 0 | <33.3 | ok | OK |
| cook: result card | 30.0 | 0 / 0 | <33.3 | ok | OK |
| cook: bake | 30.0 | 0 / 0 | <33.3 | ok | OK |
| cook: simmer | 30.0 | 0 / 0 | <33.3 | ok | OK |
| cook: counter | 30.0 | 0 / 0 | <33.3 | ok | OK |
| cook: serving | 30.0 | 0 / 0 | <33.3 | ok | OK |

**library** (`speed2_library.txt`; game drew 2631 of 2631 ticks, work mean 8844 us max 20087 us; 2D prims max 2085, vertex bytes max 153472, dropped 0, texture uploads 505, evictions 0, cached 505; work over 33333 us on 0 of 2631 ticks; worst 20087 us at tick 784 (LIB_ALMANAC); dcrun: 0 failures)

| window | logic t/s | ticks >33.3 / >50 ms | worst ms | audit 2 | verdict |
|---|---:|---:|---:|---|---|
| library: lobby | 30.0 | 0 / 0 | <33.3 | ok | OK |
| library: lobby shelf move | 30.0 | 0 / 0 | <33.3 | ok | OK |
| library: bestiary list | 30.0 | 0 / 0 | <33.3 | ok | OK |
| library: bestiary jar | 30.0 | 0 / 0 | <33.3 | ok | OK |
| library: almanac list | 30.0 | 0 / 0 | <33.3 | ok | OK |
| library: almanac list move | 30.0 | 0 / 0 | <33.3 | ok | OK |
| library: almanac profile | 30.0 | 0 / 0 | <33.3 | ok | OK |
| library: almanac face | 30.0 | 0 / 0 | <33.3 | ok | OK |
| library: weapons frames | 30.0 | 0 / 0 | <33.3 | ok | OK |
| library: weapons plan | 30.0 | 0 / 0 | <33.3 | ok | OK |
| library: items | 30.0 | 0 / 0 | <33.3 | ok | OK |
| library: items card | 30.0 | 0 / 0 | <33.3 | ok | OK |
| library: trophies | 30.0 | 0 / 0 | <33.3 | ok | OK |
| library: trophy page | 30.0 | 0 / 0 | <33.3 | ok | OK |
| library: jukebox | 30.0 | 0 / 0 | <33.3 | ok | OK |
| library: jukebox playing | 30.0 | 0 / 0 | <33.3 | ok | OK |
| library: gallery textures | 30.0 | 0 / 0 | <33.3 | ok | OK |
| library: gallery tiled | 30.0 | 0 / 0 | <33.3 | ok | OK |
| library: gallery sky | 30.0 | 0 / 0 | <33.3 | ok | OK |
| library: field guide contents | 30.0 | 0 / 0 | <33.3 | ok | OK |
| library: field guide page | 30.0 | 0 / 0 | <33.3 | ok | OK |
| library: field guide page turn | 30.0 | 0 / 0 | <33.3 | ok | OK |
| library: field guide scroll | 30.0 | 0 / 0 | <33.3 | ok | OK |

**action** (`speed2_action.txt`; game drew 3258 of 3258 ticks, work mean 4997 us max 23916 us; 2D prims max 1378, vertex bytes max 101600, dropped 0, texture uploads 355, evictions 0, cached 355; work over 33333 us on 0 of 3258 ticks; worst 23916 us at tick 2622 (WORKSHOP); dcrun: 0 failures)

| window | logic t/s | ticks >33.3 / >50 ms | worst ms | audit 2 | verdict |
|---|---:|---:|---:|---|---|
| action: level select | 30.0 | 0 / 0 | <33.3 | ok | OK |
| action: level select move | 30.0 | 0 / 0 | <33.3 | ok | OK |
| action: breach intro card | 30.0 | 0 / 0 | <33.3 | ok | OK |
| action: breach play (combat, reference) | 30.0 | 0 / 0 | <33.3 | ok | combat (ref) |
| action: breach pause menu | 30.0 | 0 / 0 | <33.3 | ok | OK |
| action: level select after a level | 30.0 | 0 / 0 | <33.3 | ok | OK |
| action: arena intro card | 30.0 | 0 / 0 | <33.3 | ok | OK |
| action: arena fight (combat, reference) | 30.0 | 0 / 0 | <33.3 | ok | combat (ref) |
| action: arena pause menu | 30.0 | 0 / 0 | <33.3 | ok | OK |
| action: arena results | 30.0 | 0 / 0 | <33.3 | ok | OK |
| action: boss rush intro card | 30.0 | 0 / 0 | <33.3 | ok | OK |
| action: boss rush round 1 (combat, reference) | 30.0 | 0 / 0 | <33.3 | ok | combat (ref) |
| action: boss rush results | 30.0 | 0 / 0 | <33.3 | ok | OK |
| action: horde intro card | 30.0 | 0 / 0 | <33.3 | ok | OK |
| action: horde results | 30.0 | 0 / 0 | <33.3 | ok | OK |
| action: mayhem intro card | 30.0 | 0 / 0 | <33.3 | ok | OK |
| action: mayhem chaos (combat, reference) | 30.0 | 0 / 0 | <33.3 | ok | combat (ref) |
| action: workshop | 30.0 | 0 / 0 | <33.3 | ok | OK |
| action: workshop part change | 30.0 | 0 / 0 | <33.3 | ok | OK |
| action: workshop armory | 30.0 | 0 / 0 | <33.3 | ok | OK |
| action: weld intro card | 30.0 | 0 / 0 | <33.3 | ok | OK |
| action: welding | 30.0 | 0 / 0 | <33.3 | ok | OK |

**dream** (`speed2_dream.txt`; game drew 1482 of 1483 ticks, work mean 8474 us max 14879 us; 2D prims max 1455, vertex bytes max 119552, dropped 0, texture uploads 411, evictions 0, cached 411; work over 33333 us on 0 of 1482 ticks; worst 14879 us at tick 1368 (DREAM); dcrun: 0 failures)

| window | logic t/s | ticks >33.3 / >50 ms | worst ms | audit 2 | verdict |
|---|---:|---:|---:|---|---|
| dream: bedroom | 30.0 | 0 / 0 | <33.3 | ok | OK |
| dream: thought bubble | 30.0 | 0 / 0 | <33.3 | ok | OK |
| dream: vignette 1 cute | 30.0 | 0 / 0 | <33.3 | ok | OK |
| dream: vignette 1 twist | 30.0 | 0 / 0 | <33.3 | ok | OK |
| dream: vignette 2 | 30.2 | 0 / 0 | <33.3 | ok | OK |
| dream: nightmare vignette 1 | 30.0 | 0 / 0 | <33.3 | ok | OK |
| dream: nightmare vignette 2 | 30.0 | 0 / 0 | <33.3 | ok | OK |
| dream: nightmare vignette 3 (cute, then red) | 30.0 | 0 / 0 | <33.3 | ok | OK |
| dream: wake-up choice | 30.0 | 0 / 0 | <33.3 | ok | OK |

**house** (`speed2_house.txt`; game drew 2461 of 2461 ticks, work mean 3512 us max 20511 us; 2D prims max 1146, vertex bytes max 76256, dropped 0, texture uploads 147, evictions 0, cached 147; work over 33333 us on 0 of 2461 ticks; worst 20511 us at tick 42 (HOME); dcrun: 0 failures)

| window | logic t/s | ticks >33.3 / >50 ms | worst ms | audit 2 | verdict |
|---|---:|---:|---:|---|---|
| house: build mode | 30.0 | 0 / 0 | <33.3 | 115 over, worst 40.2 | OK |
| house: build cursor move | 30.0 | 0 / 0 | <33.3 | 27 over, worst 39.8 | OK |
| house: catalog modal | 30.0 | 0 / 0 | <33.3 | 65 over, worst 49.1 | OK |
| house: catalog cursor move | 30.0 | 0 / 0 | <33.3 | 21 over, worst 46.8 | OK |
| house: held piece | 30.0 | 0 / 0 | <33.3 | 53 over, worst 40.5 | OK |
| house: rotate view | 30.0 | 0 / 0 | <33.3 | 26 over, worst 40.0 | OK |
| house: photo mode | 30.0 | 0 / 0 | <33.3 | 74 over, worst 42.8 | OK |
| house: photo snap | 30.0 | 0 / 0 | <33.3 | 25 over, worst 46.6 | OK |
| house: live mode | 30.0 | 0 / 0 | <33.3 | 105 over, worst 43.6 | OK |
| house: score panel | 30.0 | 0 / 0 | <33.3 | 69 over, worst 46.1 | OK |
| house: live, 4 chibis (room-test 1) | 30.0 | 0 / 0 | <33.3 | 105 over, worst 43.6 | OK |
| house: defend tutorial | 30.0 | 0 / 0 | <33.3 | 109 over, worst 42.1 | OK |
| house: defend siege wave 3 | 30.0 | 0 / 0 | <33.3 | 108 over, worst 42.6 | OK |
| house: defend siege freeze + double clock | 30.0 | 0 / 0 | <33.3 | 74 over, worst 42.8 | OK |
| house: visit (Nana) | 30.0 | 0 / 0 | <33.3 | 0 over, worst <33.3 | OK |
| house: homemart | 30.0 | 0 / 0 | <33.3 | 0 over, worst <33.3 | OK |
| house: homemart cursor move | 30.0 | 0 / 0 | <33.3 | 0 over, worst <33.3 | OK |
| house: homemart season tab | 30.0 | 0 / 0 | <33.3 | 0 over, worst <33.3 | OK |
| house: black market | 30.0 | 0 / 0 | <33.3 | 0 over, worst <33.3 | OK |

**studio** (`speed2_studio.txt`; game drew 1165 of 1165 ticks, work mean 7176 us max 30403 us; 2D prims max 1260, vertex bytes max 91008, dropped 0, texture uploads 351, evictions 0, cached 351; work over 33333 us on 0 of 1165 ticks; worst 18296 us at tick 234 (STUDIO); dcrun: 0 failures)

| window | logic t/s | ticks >33.3 / >50 ms | worst ms | audit 2 | verdict |
|---|---:|---:|---:|---|---|
| studio: choir quick fill | 30.0 | 0 / 0 | <33.3 | ok | OK |
| studio: choir member card | 30.0 | 0 / 0 | <33.3 | ok | OK |
| studio: citizen picker | 30.0 | 0 / 0 | <33.3 | ok | OK |
| studio: citizen picker move | 30.0 | 0 / 0 | <33.3 | ok | OK |
| studio: stage tab | 30.0 | 0 / 0 | <33.3 | ok | OK |
| studio: file tab | 30.0 | 0 / 0 | <33.3 | ok | OK |
| studio: song book | 30.0 | 0 / 0 | <33.3 | ok | OK |
| studio: templates | 30.0 | 0 / 0 | <33.3 | ok | OK |
| studio: templates move | 30.0 | 0 / 0 | <33.3 | ok | OK |

**creator** (`speed2_creator.txt`; game drew 2335 of 2335 ticks, work mean 8442 us max 15994 us; 2D prims max 1403, vertex bytes max 102304, dropped 0, texture uploads 422, evictions 0, cached 422; work over 33333 us on 0 of 2335 ticks; worst 15994 us at tick 2312 (CREATOR); dcrun: 0 failures)

| window | logic t/s | ticks >33.3 / >50 ms | worst ms | audit 2 | verdict |
|---|---:|---:|---:|---|---|
| creator: species and body tab | 30.0 | 0 / 0 | <33.3 | ok | OK |
| creator: tab change to head and face | 30.0 | 0 / 0 | <33.3 | ok | OK |
| creator: head and face tab | 30.0 | 0 / 0 | <33.3 | ok | OK |
| creator: tab change to hair | 30.0 | 0 / 0 | <33.3 | ok | OK |
| creator: hair tab | 30.0 | 0 / 0 | <33.3 | ok | OK |
| creator: tab change to supernatural | 30.0 | 0 / 0 | <33.3 | ok | OK |
| creator: supernatural tab | 30.0 | 0 / 0 | <33.3 | ok | OK |
| creator: tab change to outfit | 30.0 | 0 / 0 | <33.3 | ok | OK |
| creator: outfit tab | 30.0 | 0 / 0 | <33.3 | ok | OK |
| creator: tab change to voice | 30.0 | 0 / 0 | <33.3 | ok | OK |
| creator: voice tab | 30.0 | 0 / 0 | <33.3 | ok | OK |
| creator: tab change to personality | 30.0 | 0 / 0 | <33.3 | ok | OK |
| creator: personality tab | 30.0 | 0 / 0 | <33.3 | ok | OK |
| creator: tab change to profile | 30.0 | 0 / 0 | <33.3 | ok | OK |
| creator: profile tab | 30.0 | 0 / 0 | <33.3 | ok | OK |
| creator: tab change to home | 30.0 | 0 / 0 | <33.3 | ok | OK |
| creator: home tab | 30.0 | 0 / 0 | <33.3 | ok | OK |
| creator: grid (first row) | 30.0 | 0 / 0 | <33.3 | ok | OK |
| creator: grid cursor move | 30.0 | 0 / 0 | <33.3 | ok | OK |
| creator: hat grid (bust camera) | 30.0 | 0 / 0 | <33.3 | ok | OK |
| creator: hat grid try-on move | 30.0 | 0 / 0 | <33.3 | ok | OK |
| creator: hat grid filter chip | 30.0 | 0 / 0 | <33.3 | ok | OK |
| creator: colour swatches | 30.0 | 0 / 0 | <33.3 | ok | OK |
| creator: swatch move | 30.0 | 0 / 0 | <33.3 | ok | OK |
| creator: start menu | 30.0 | 0 / 0 | <33.3 | ok | OK |

**life** (`speed2_life.txt`; game drew 3918 of 3919 ticks, work mean 8905 us max 4474135 us; 2D prims max 2754, vertex bytes max 224928, dropped 0, texture uploads 621, evictions 0, cached 621; work over 33333 us on 2 of 3918 ticks; worst 4474135 us at tick 1968 (SAVELOAD); dcrun: 0 failures)

| window | logic t/s | ticks >33.3 / >50 ms | worst ms | audit 2 | verdict |
|---|---:|---:|---:|---|---|
| life: new island -> hub (the load) | 30.4 | 1 / 1 | 97.7 | 1 over, worst 97.7 | MISS 1x |
| life: family dashboard | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: add member: kind | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: add member: pick a resident | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: add member: pick move | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: add member: role | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: family with a member | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: play-as switcher | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: play-as move | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: options controls | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: options page turn to shooter | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: options shooter | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: options page turn to life | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: options life | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: options page turn to audio | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: options audio | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: options page turn to display | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: options display | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: options page turn to performance | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: options performance | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: options page turn to accessibility | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: options accessibility | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: options page turn to cheats | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: options cheats | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: options page turn to game | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: options game | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: options cursor move | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: save/load | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: save/load cursor move | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: saving (the save freeze) | 30.0 | 1 / 1 | 4474.1 | 1 over, worst 4473.9 | MISS 1x |
| life: care popup (feed) | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: care popup, toast gone | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: care reaction | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: shop (food) | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: shop cursor move | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: shop quantity popup | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: shop gifts tab | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: black market | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: cafe counter | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: news help wanted | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: news third tab | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: graveyard | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: unlocks | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: unlocks cursor move | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: unlocks row moves | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: unlocks paints tab | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: citizen picker | 30.0 | 0 / 0 | <33.3 | ok | OK |
| life: citizen picker move | 30.0 | 0 / 0 | <33.3 | ok | OK |

**story** (`speed2_story.txt`; game drew 3206 of 3208 ticks, work mean 6395 us max 99688 us; 2D prims max 4524, vertex bytes max 292032, dropped 0, texture uploads 665, evictions 0, cached 665; work over 33333 us on 1 of 3206 ticks; worst 99688 us at tick 562 (CUTSCENE); dcrun: 0 failures)

| window | logic t/s | ticks >33.3 / >50 ms | worst ms | audit 2 | verdict |
|---|---:|---:|---:|---|---|
| story: first boot language picker | 30.0 | 0 / 0 | <33.3 | ok | OK |
| story: language picker move | 30.0 | 0 / 0 | <33.3 | ok | OK |
| story: first boot security code | 30.0 | 0 / 0 | <33.3 | ok | OK |
| story: security code digit | 30.0 | 0 / 0 | <33.3 | ok | OK |
| story: title -> new story (the load) | 31.4 | 1 / 1 | 99.7 | 1 over, worst 99.7 | MISS 1x |
| story: prologue cutscene | 30.0 | 0 / 0 | <33.3 | ok | OK |
| story: quest log story tab | 30.0 | 0 / 0 | <33.3 | ok | OK |
| story: quest log side tab | 30.0 | 0 / 0 | <33.3 | ok | OK |
| story: quest log side cursor move | 30.0 | 0 / 0 | <33.3 | ok | OK |
| story: quest log items tab | 30.0 | 0 / 0 | <33.3 | ok | OK |
| story: quest log map tab | 30.0 | 0 / 0 | <33.3 | ok | OK |
| story: quest log map cursor move | 30.0 | 0 / 0 | <33.3 | ok | OK |
| story: quest log friends tab | 30.0 | 0 / 0 | <33.3 | ok | OK |
| story: quest log trophies tab | 30.0 | 0 / 0 | <33.3 | ok | OK |
| story: cutscene chapter card 2 | 30.0 | 0 / 0 | <33.3 | ok | OK |
| story: cutscene chapter card 3 | 30.0 | 0 / 0 | <33.3 | ok | OK |
| story: cutscene chapter card 4 | 30.0 | 0 / 0 | <33.3 | ok | OK |
| story: cutscene home vignette | 30.0 | 0 / 0 | <33.3 | ok | OK |
| story: cutscene fog vignette | 30.0 | 0 / 0 | <33.3 | ok | OK |
| story: cutscene eruption vignette | 30.0 | 0 / 0 | <33.3 | ok | OK |
| story: cutscene vapula vignette | 30.0 | 0 / 0 | <33.3 | ok | OK |
| story: cutscene after a press | 30.0 | 0 / 0 | <33.3 | ok | OK |
| story: credits | 30.0 | 0 / 0 | <33.3 | 21 over, worst 35.5 | OK |
| story: credits parade | 30.0 | 0 / 0 | <33.3 | 72 over, worst 62.2 | OK |

## 3. Ranked misses

Order: slow-motion logic, then a steady low frame rate, then periodic hitches, then single long frames. "ms" in
the cause column is per drawn frame, from the in-window profiles. "Touched by" names the landed package.

### 3.1 GBA

| rank | scene(s) | now | audit 2 | cause (profile) | touched by |
|---:|---|---|---|---|---|
| 1 | Shooting Gallery play | 11.0 / 10.5 fps, logic 19.4 / 18.5, 85-87 ms | the same | the whole booth is drawn again every frame: `vid_rect_narrow` 27.5 ms, `out_rect` 11, `ellipse_draw` 6, `nested_bands` 5.9, rect call glue 6.2, outlined HUD text 4.6 (of a 91-95 ms frame) | nothing (S2-4 did Whack only) |
| 2 | Dreams: vignettes, nightmare, wake-up choice, thought bubble | 12.7-13.2 fps (logic 23.9-24.5); nightmare 10.6-12.3 (17.6-22.5); choice 10.6 (17.6); bubble 22.5 (28.1) | the same | an animated sky (5 wavy bands, a `ui_wave` per 8 px column), a scrolling floor, the bubble frame: rects 28-33 ms, shapes 11.5-17, divisions 4.4-6.4, checkers 3.4-5; the full-screen dim `vid_shade_words` 14-15 ms in the red nightmare and the choice | nothing |
| 3 | Fog vignette | 10.0 fps, logic 18.4, 92.8 ms | the same | rects 34 ms, shapes 29 (`ellipse_draw` 12%, `out_rect` 10%: the fog puffs), `vid_blit` 19 | nothing |
| 4 | Concert band; setup screen | band 13.2 fps, logic 23.6 (56.7 ms); setup 14.5, logic 28.3 (60.1 ms) | not in audit 2 (SP6: 12.4 / 13.4) | band: blits 14 ms, rects 14, `concert_draw_stage` 10 + `wall_rect` 4 (the flame wall), beams and checkers 9. Setup: `vid_shade_words` 22 ms (the dim over the live stage) | SP6, SP2 |
| 5 | Island hub with a tip card up | 19.0 fps, logic 23.8, frames to 117 ms | CL4: 20.7 | the hub keeps drawing map parts under the card: `vid_rect_narrow` +9.6 ms, `fp_island_draw_map_part` +3.8, `lr_run` +2.9 in the slow frames | SP3 (not under a card) |
| 6 | In flight: workshop; the save; New story; new island | workshop 18.7 fps, logic 25.6, three 260 ms frames; save 11.4 s; New story 2.07 s; new island 1.99 s, then 39 frames of 67-117 ms | the same | `outline` + `seg` (weapon composition); flash writes and town generation in one tick; island map parts (rects 27 ms a frame) | S2-9, S2-10 (in flight) |
| 7 | Creator: hat try-on move | 15.2 fps, logic 28.6 | the same | scaled preview `vid_blit_c` 10 ms, panel recapture (`pn_route` + `plat_rect_slow` 10 ms on a move), portrait slices 7.7, `lock_compose` 3.6 | nothing (S2-7 left it) |
| 8 | Roster cursor moves (SP4 tour) | 19.8 fps, frames of 65-95 ms | SP4: 19.8 | a whole chibi drawn in one tick (`finish_marked` 9.4 ms, `sh_span` 5.4, `scan_transitions` 3.9), once with the sim pass on top | SP2 (stand-ins need an older picture of the same look) |
| 9 | Island town hall menu, first seconds | 26.2 fps, 16 frames over 33 ms (9 over 50, worst 117) | SP2: 26.2 | map parts still being built under the menu (`lr_run` +6.8 ms, `fp_island_draw_map_part` +3.4, `put` +3.0) and portrait slices 7.8 ms a frame | SP2, SP3 |
| 10 | Story map entry (ch1 overworld) | 24.2 fps, logic 26.5, one 251 ms frame, then 18 frames of 50-94 ms | G-LD note | the entry frame, then the sim's first pass: `update_resident_ex` 11 ms, `decide_activity` 6.8, `memmove` 4.4 | SP2 (X4), SP5 (fade) |
| 11 | Fun Fair play: Whack, Dodge, Derby, Memory Hellfire | Whack 20.9 / 20.0; Dodge 22.5 / 27.2 / 20.7; Derby 22.1-22.6; Memory Hellfire 21.5 | Whack 15.1; Derby 15.0; the others the same | Whack (48 ms frame): live parts' rects 10-12 ms, shapes 9-10, the whole-picture copy `plat_stage_restore` 5 + `stage_rows` 3.3, divisions 3.3, HUD text 2.5-4.3. Dodge: rects 14-17, shapes 9-13 (backdrop and clouds), `dg_bot` 1.8-3.6. Derby: rects 11, divisions 5.1-5.7, HUD text 3.3-4.5, racers 3.4. Memory: card rects 17-20 (`vid_rect` 10-11.5), shapes 7-9 | S2-4 (Whack), S2-5 (Derby racers) |
| 12 | Kitchen: all nine windows | book 20.0, chop 21.9, stir 20.0, fry 20.3, bake 20.8, simmer 22.4, result card 17.0, counter 18.7, serving 19.0 | the same | stations: rects 21-23 ms, shapes 9 (`kitchen_draw_back`: wallpaper, tiles, window; it stands still). Book: rects 25, shapes 10, text 6.7. Result card: rects 28, shapes 13. Counter / serving: rects 17-18, blits 5.3, chibi renders 4.3-5.7 (and 7 frames of 60-79 ms) | nothing |
| 13 | Fishdex; catch card | Fishdex 15.0 (56 ms); catch card 25.6 (14 frames of 67-84 ms) | the same | Fishdex: the fish drawn by maths every frame: `fish_draw` 9.4 ms, `fx_isqrt` 6.0, divisions 6.4, rects 17. Card: the full-screen dim on each pop-in frame, `vid_shade_words` +15.3 ms | S2-1 (not held: animated scenes) |
| 14 | Story cutscenes: card 2, card 4, home, eruption, Vapula, after a press, P0 bridge, gate cracks | 15.0; 22.7; 20.0; 16.9; 16.1; 20.0; 20.3; 28.2 | the same but Vapula (12.0) and after a press (17.9) | card 2: rects 29 ms, shapes 22 (puff clouds). Card 4: 12 frames of 83-87 ms, the wide title rasterised again (`text_emit` 23 ms, `ur_run` 11.6, `em_finish_row` 10). Home: the full-screen wallpaper through `vid_blit`, 29 ms. Eruption: shapes 18, rects 13, blits 12.6. Vapula: sky rects 20, the demon 15.8. Bridge: `plat_line_m4` 15 ms | S2-5 (Vapula, home egg) |
| 15 | Concert solo, choir | solo 22.2 (37.0 ms); choir 15.0 (46.9); tour 1 concert 16.2 | SP6+SP2: 20.5; 14.6 | solo: blits 11.5 ms, rects 8.5, beams + checkers 7.9. Choir: rects 13, blits 12.6, flame wall 15, beams + checkers 8.4 | SP6, SP2 |
| 16 | Exorcism, every phase | intro 22.7, tools 28.3 (21.5 at the end of tour 1), prayer 19.2, holy water 23.4, true name 19.5, verdict 19.7, result 19.1 | SP7: 17.8, 20.4, 15.4, 21.1, 17.5, 16.4, 17.3 | putting the backdrop back 4.4-10.5 ms (`stage_rows` 8-13%, `plat_stage_restore` 6-9%, `restore_dirty` 3-6%), figure blits 4.6-6.5, rects 4.3-6.7, shapes 2.5-5.4, portrait jobs 1.1-7.7, dims: verdict 15.8 ms, result 7 | SP7, SP2 |
| 17 | Creator with input held | value step 22.2-23.2; grid scroll 17.2; part picker 21.8; filter chip 21.7; rows 27.2-29.4 | SP2: 22.3; 18.1 | every step recaptures a panel (`pn_route` 3.1-5.5 ms, `plat_rect_slow` 1.8-3.2: each 2D call is checked while a panel is captured) and renders a new look (portrait 4.5-6.9 ms); the scaled preview goes through `vid_blit_c` (10 ms a redraw) | CR9, SP2 |
| 18 | Library lobby, jukebox, Almanac list move | lobby 19.8 / 19.3; jukebox 20.0 / 19.8; list move 25.4 | the same | lobby: rects 17 ms, text 5.4, shapes 4.9. Jukebox: rects 14, shapes 9.5 (the record: `ellipse_draw` 7%), text 6.5. List move: the list and the preview drawn whole (rects 10.8, text 6.3) | S2-1, S2-8 (the cards and pages, not these) |
| 19 | Studio STAGE tab | 22.6 (36.3 ms) | 22.8 | rects 11 ms, blits 6.3 (`vid_blit` 12%), shapes 5.9, `sc_replay` 5.4% | nothing |
| 20 | Level select | 29.7-30.0 with 2-3 frames of 50 ms (33.3 ms average); the slide 16.9 fps | 18.8; 15.0 | outlined text 10.6 ms (`vid_text_dil` + `vid_glyph_dilate` 19%), rects 10.3, the held preview put back 2.5; the slide renders a preview per step (`plat_view3d` 10 ms + `vh_pack_row` 8.2) | S2-2 |
| 21 | Care reaction; add-member wizard | reaction 19.0 (44.5 ms); wizard 25.2-27.0 (34-35 ms) | 17.3; 25.5-27.0 | the resident / family screen drawn whole under the bubble or the choice box: rects 9-11 ms, text 5-9, shade 4-4.6, shapes 3-4.4 | S2-1 (left both) |
| 22 | Credits parade; credits | parade 26.3 (32 frames of 50 ms); credits 29.3 (6) | 5.2; 7.2 | overworld 12.7 ms, card and ticker text 4.2, body blits 3.4, body job slices 2.8 | S2-7 |
| 23 | In flight: workshop part change, armory | 23.4; 26.7 | the same | rects 10-10.6 ms, text 5.8-8.8, `outline` + `seg` | S2-9 (in flight) |
| 24 | **The sim's 15-minute pass** (periodic, every free-play screen) | every 1,130 ticks (37.7 s) about 17 drawn frames of 50-67 ms: shop gifts tab 21.5 fps in its window, citizen picker 22.9, apartments 25.7, Options Accessibility 23.7 / 28.2, pet 28.5 (5 frames of 67-84 ms), pet-care boss 28.6 | gifts tab 30.0, picker 29.7 (the pass fell elsewhere: Black Market 22.9) | all 1,000 residents come due in the same 32 ticks (`SIM_MIN_ELAPSED` 15 game minutes, every stamp set to the same minute): `update_resident_ex` 10.7-14.7 ms a tick, `memmove` 2.6-4.7, `gene_get` + `res_get_persona` + `sim_check_problems` 3-4 | SP2 (X4 made one update cheaper) |
| 25 | Creator hat grid; swatch move | grid 27.2 (14 frames of 50-67 ms); swatch 26.0 (one 100 ms frame) | the same | grid: panel recapture (`pn_route` +9.6 ms, `plat_rect_slow` +5.2). Swatch: a whole portrait in one tick (`finish_marked` 17.6 ms, `capsule_plain` 9, `sh_span` 8.7, `scan_transitions` 6.4) | nothing |
| 26 | House: busy Live layout, Hold the Door wave 3, the stress layouts | busy Live 26.8 (27 frames of 50 ms); wave 3 29.5 (4); `home_steady` 29.2; cellar 26.2; defend crowd 12.6 fps, logic 23.1 | the same | `draw_region` 18% (+4.7 ms on the slow frames); cellar: `draw_region` +12, `floor_row_lit` +5.7; defend crowd: the room 50 ms a frame | H2, S2-1 (modals) |
| 27 | Title; title menu open | 28.6 (5 frames of 67-84 ms); 26.8 (16 frames) | SP2: 28.6 | figure renders forced into full frames: `capsule_plain` 10 ms, `finish_marked` 7.9, `sh_span` 6.1 in the slow ones | SP2 |
| 28 | Menus within 3 ms of the budget | news 29.3 (6 frames of 50 ms), resident 29.3-29.4 (5-6), Unlocks row moves 28.9 (one per row change), pet health 29.7, exorcism tools, Catch Hellfire (3), Memory Sweet (4), fishing cast (3), pause 29.5 (4) | mostly the same | the frame is 30-33 ms before anything extra: text 5-9 ms, window rects 8-11, shapes 3-4, shadows 2-3.3. A portrait slice, a panel recapture or a toast tips it | SP2, SP4, S2-6 |
| 29 | House view turn | one 117 ms frame a turn (work 106 ms); entering the house pays it twice (158 + 129 ms) | the same | `room_roof_slope` 22.9 ms, `build_shell` 22.5, `room_roof_pattern` 13.3, `draw_region` 6.4, `row_copy` 5.1, `roof_row` 4.8 | nothing (S2-11 not built) |
| 30 | List moves with a preview | citizen picker move 95 ms, add-member pick move 95 ms, roster 65-92 ms, studio picker move 44 ms | the same | a new citizen's picture is drawn at once: `sh_span` 16.5 ms, `finish_marked` 15.1, `scan_transitions` 5.2, `pc_*` 10, `capsule_plain` 3.9 | S2-7 said "2D drawing": the profile says a chibi render |
| 31 | Creator tab changes | 1-2 frames of 50-67 ms on the press, and a 101 ms frame about a second later | the same (the 101 ms frame is new: it lies in the settle) | press: `pn_route` 7-13 ms, `plat_rect_slow` 3.3-5.6, `vid_blit_c` 3.4. Later: `finish_marked` 14.5, `vid_blit_c` 10.1, `sh_span` 9.9 | nothing |
| 32 | Wheel result card | first frame 67 ms, then the card at 15 fps for 2 s | 6.5 fps | full frames while the confetti lasts (`vid_rect_narrow` +10 ms) | S2-3 |
| 33 | One or two 50 ms frames | shop (food), Black Market, cafe, roster, studio picker, templates, quest log side cursor, overworld pause, invaded, P0 fireworks, cutscene wall, LYRICS, CHOIR tab | the same | a forced portrait slice in a frame with no room (`capsule_plain` +1.2-6, `finish_marked` +1-6 ms), or a panel recapture (`pn_route` +0.9-1.4) | SP2 |

### 3.2 PS1

239 windows besides combat: 221 OK, 18 miss. Of the 214 speed tour 2 windows 201 are OK (audit 2: the fair hung
at Dodge; credits 10.9 and 1.5 fps; wheel 22.4; Hell Breach card 22.9; chapter card 2 26.7: all of these hold 30
now). The exorcism holds 30.00 in every phase on this Redux build (SP7 measured 27.1-30).

| rank | scene | now | audit 2 | cause | package |
|---:|---|---|---|---|---|
| 1 | In flight: the save | one 11.6 s frame | the same | the memory card write in one tick | S2-10 |
| 2 | In flight: title to New story; new island | New story: frames of 2.42 s and 1.18 s and 7 overlay reads of 314-369 ms. New island: one 1.87 s frame and 4 reads, then 2 frames of 50 ms (audit 2: 40) | 2.33 s + 1.13 s; 1.77 s | town generation in one tick; `OVL.BIN` reads | S2-10 |
| 3 | In flight: workshop | 28.78 fps, three 100 ms frames | the same | `seg` + `outline` (the weapon composition) | S2-9 |
| 4 | House, busy Live layout (room-test 1) | 28.89 fps: 8 frames of 50 ms, one of 67 | the same | the 3D room with four walkers: `draw_piece` 15.6%, `g4` 12.2%, `tri` 5.9%, `g3` 5.6%, `room_render` 5.2%, `lit` 4.8%, `tone` 4.8% (CPU-bound in Redux) | the owner decision of 2026-11-12 (no 3D character models inside houses: chibis as billboards) takes the walkers' polygons out; measure again after it |
| 5 | Creator: hat try-on move; grid scroll; value held | 28.57 fps (2 frames of 50 ms, one of 67); `creator_speed.txt`: grid scroll 29.39 (6 frames of 50), value 29.88 (3) | 28.57; CR9: floors only | try-on: `part_icon_unpack` 30% of the samples (icons unpacked again on each step), `rect_slow` 6%, `memcpy` 5% | S3-3 (PS1 part: keep the page's unpacked icons in the creator's overlay scratch) |
| 6 | Dialogue (`perf_dialogue`); island hub | dialogue 29.67 (2 real frames of 50 ms in 90); hub 29.83-29.88 (1 in 90-120) | SP2: 29.7 | portrait slices in a full frame (not profiled: one sample a vsync cannot name one frame) | S3-2 (`ps1/src/pslice.c`) |
| 7 | Single 67 ms frames | lobby shelf move, Almanac list move, nightmare vignette 2, wake-up choice, credits (one each); kitchen counter and care reaction one 50 ms frame | the same; care reaction is new | on the GBA the same moments are a whole chibi drawn in one tick | S3-2 |
| - | every scene change | 98-502 ms of overlay reads a group, two groups on most changes (176 reads in this audit) | 260-630 ms | `OVL.BIN` from the drive; SP5 darkens the screen over it | S3-15 |

Everything else holds 30 with no frame over 33 ms: all seven fair games, fishing and cooking, every library
wing, every action card and menu, dreams, the house (Build, catalog, Photo, Live, Score, Hold the Door, visit,
HomeMart), every creator tab, the life screens, the quest log, the cutscenes, the studio, the title, pet, concert
and the exorcism. Combat (reference): 29.45-30.00 fps (`perf_combat`), Monster Mayhem 29.83.

### 3.3 Dreamcast

214 windows besides combat: 211 OK. The house (audit 2: 1,487 ticks over budget) holds 30 since DC8: worst tick
20.5 ms. The credits hold 30 since S2-7. Every group drew every tick (the save aside); no 2D prim was dropped.

| rank | scene | now | audit 2 | cause | package |
|---:|---|---|---|---|---|
| 1 | In flight: the save | one 4.47 s tick | the same | the VMU write in one tick | S2-10 |
| 2 | In flight: title to New story; new island | one tick of 99.7 ms; one of 97.7 ms | the same | town generation in one tick | S2-10 |

Worst tick per group otherwise: fair 15.9 ms (Derby), fishcook 18.1, library 20.1 (Almanac), action 23.9
(workshop), dream 14.9, house 20.5, studio 18.3, creator 16.0. Nothing to plan for the Dreamcast besides S2-10
and DC6 itself (the DC stream).

### 3.4 GBA: long frames outside the windows (scene entries and the second after a press)

These lie in the settle before a window, so the tables do not count them. One frame each. A loading fade covers
the ones over 150 ms (SP5).

| where | frame | cause (profile of that frame) | package |
|---|---:|---|---|
| studio: closing the citizen picker (back to the choir row) | 325 ms | about five chibis drawn at once: `finish_marked` 58 ms, `sh_span` 38, `capsule_plain` 33, `sh_load` 20, `scan_transitions` 17 | S3-2 |
| combat start (Hell Breach, arena, arena modes) | 335-402 ms | level set-up and the first 3D frame (SP5 fades over it) | combat: not planned |
| workshop entry and part changes | 251-318 ms | weapon composition | S2-9 (in flight) |
| quest log: Friends tab | 230 ms | three portraits at once: `finish_marked` 39 ms, `sh_span` 22, `capsule_plain` 19 | S3-2 |
| Fun Fair: a title card, a game start | 100-201 ms | big outlined text rasterised on its first frame (`text_emit` 27 ms, `em_finish_row` 10, `ur_run` 9) plus the whole booth | S3-12 |
| house entry; a view turn | 158 + 129 ms; 117 ms | the roof and shell tables (rank 29) | S3-10 |
| creator entry; a tab change's second | 162 ms; 101 ms | a portrait at once (`finish_marked` 17.6 ms, `capsule_plain` 9) + `palette_nearest` 11 | S3-2, S3-3 |
| level select entry | 127 ms (201 with the scene change) | the first preview: `plat_view3d` 44 ms | S3-8 |
| shop, cafe, Black Market, play-as, picker, care popup entry | 106-167 ms | the keeper's or citizen's portrait at once (`finish_marked` 19.7 ms, `capsule_plain` 10.9) | S3-2 |
| action intro cards | 131-201 ms | the card's first frame with the 3D view under it | S3-12 (text part) |

Entries with a demo arg (`LIBRARY 256`, `DREAM -2`, `STUDIO -107`, `COOKING 129`, `HOME 3`) and `scene STORY n`
take 1.7-2.3 s in the scripts. That is the harness's town set-up, or the story load that S2-10 owns.

## 4. Fix packages

Size: S (a day or less), M (a few days), L (a week or more). Savings are per drawn frame, from the measured shares.
They are estimates, not measurements. "Exact" = the same pixels on every frame. "Pixel-changing" needs to fit the
owner's SPEED DEFAULTS (held frames, stand-ins, short fades) or an owner yes. GBA room on this tree: EWRAM 1,788 B
free, IWRAM main stack 6,308 B (the overlay slot is full with the 3D group; the UI group has 6,436 B of the slot
free), ROM 8.0 of 32 MB. PS1: 64 KB below the stack top, under the gate: nothing below adds PS1 resident bytes.
"Stream" is the owner under `held/PARALLEL_WAVES.md`.

**Covered by what is in flight, so not planned here:** the workshop's 260 ms frames, part change and armory
(S2-9); the save's 11.4 s frame, title to New story 2.07 s, the new island's 1.99 s frame (S2-10); the PS1 margin
(PS1-RAM). Left over next to them: the 39 frames of 67-117 ms after the new-island load (S3-14) and the scene
entries of 100-330 ms (3.4).

| ID | what | lifts (now -> expected) | files (stream) | size | exact? | RAM / ROM |
|---|---|---|---|---|---|---|
| **S3-1** | **The sim's 15-minute pass, spread.** `sim_init`, load and fast-forward set every resident's stamp to the same minute, so all 1,000 come due in the same 32 ticks every 15 game minutes. Stagger the stamps over the 15 minutes (slot mod 15), so about 67 residents are due per game minute | every free-play screen: 17 frames of 50-67 ms every 37.7 s -> none (16-22 ms a tick in the pass -> about 1 ms): shop gifts tab 21.5 -> 30, citizen picker 22.9 -> 30, apartments 25.7 -> 30, Options Accessibility 23.7 / 28.2 -> 30, pet's 5 frames of 67-84 ms -> 0, pet-care boss 28.6 -> 30. Helps the story-map entry second in part | `core/src/sim.c`, the sim goldens (SPEED) | S | no pixels change; the sim's random details do (another update order, the same rates): `test_sim_x4` and other sim goldens are recorded again | 0 B |
| **S3-2** | **No whole portrait or chibi inside a drawn frame.** (a) A brand-new look with nothing to stand in is "drawn at once" today: on the GBA and PS1 a UI preview starts a sliced job instead and shows the previous picture or an empty frame for the 4-12 ticks it takes; list cursors prefetch their neighbours in idle time (as the dialogue does). (b) A scene's first faces are prefetched under the loading fade, or come in one per few ticks (S2-7's lazy bodies for the studio choir row and the quest log's friends). (c) `pslice.c`: a forced slice only when it fits under the vblank; dear slices cut finer. (d) The title's figures as patient jobs | studio picker close 325 ms, Friends tab 230 ms, creator entry 162 ms, shop / cafe / picker entries 106-167 ms, picker and add-member moves 95 ms, swatch move 100 ms, roster cursor 65-92 ms, kitchen counter 60-79 ms, lobby 64-70 ms: all to under 34 ms. Title 28.6 -> 30 (5-16 frames of 67-84 ms -> 0). The one or two 50 ms frames in shop, Black Market, cafe, roster, resident, pause, quest log, studio picker -> 0 | `core/src/portrait.c`, `gba/src/pslice.c`, `ps1/src/pslice.c` (CREATOR area: agree first); callers `fp_roster.c`, `fp_common.c` (the picker), `fp_family.c`, `fp_shop.c`, `story_questlog.c`, `lib_almanac.c`, `lib_common.c`, `cook_scene.c`, `studio*.c`, `creator_draw.c`, `fp_title.c` (after S2-10) | M | pixel-changing: a stand-in or an empty preview for a few ticks (SPEED DEFAULTS: stand-ins) | 0 B (the job and prefetch slots exist); a second prefetch slot would be about 40 B EWRAM |
| **S3-3** | **Creator: panels captured on the fast path, the preview through the UI group.** A frame that recaptures a creator panel pays `pn_route` 3.1-13.3 ms + `plat_rect_slow` 1.5-5.6 + `vid_route` 0.6-2.5. Use S2-6's recipe (a new key: draw first on the fast path, then a solid panel). `plat_ui_loops()` before the scaled preview (S2-5's `vid_blitn`: 10.1 -> about 3.5 ms a redraw). The lock overlay composed once a page (`lock_compose` 2.9-3.6 ms a frame). PS1 part: the hat grid unpacks its part icons again on every step (`part_icon_unpack` 30% of the samples): keep the page's icons in the creator's overlay scratch | tab changes: frames of 50-67 ms -> at most one 50 ms frame; value held 22.2-23.2 -> about 28 (30 with S3-2); rows 27.2-29.4 -> 30; hat grid 27.2 -> 30; filter chip 21.7 -> about 28; part picker 21.8 -> about 27; grid scroll 17.2 -> about 24; try-on 15.2 -> about 22. PS1: try-on 28.6 -> 30, grid scroll 29.4 -> 30 | `core/src/creator_draw.c`, `creator_grid.c`, `creator.c` (CREATOR: this is its G2-CR) | M | exact (`test_panel` with the cache on and off) | 0 B |
| **S3-4** | **Fun Fair: the kept playfield for the other games** (rest of S2-4, family 1). Gallery: the booth baked per key. Dodge: tent and ground kept, clouds live. Derby: divisions through steppers (5.1-5.7 ms), the HUD text as a kept strip (3.3-4.5), sky / grandstand / bunting kept as a wide strip. Memory: the card grid kept, a flip redraws one card. Whack: copy back only what the live parts covered (the whole-picture copy is 5 ms + `stage_rows` 3.3), ring rows without `fx_isqrt` and divisions (3.3), HUD strip. The wheel's result card over a kept card | Gallery 85-87 ms -> about 28-35: 11 -> 24-30 fps, logic 19 -> 30. Dodge 38.6-42.1 -> about 30: 21-27 -> 27-30. Derby 38 -> about 27: 22 -> 30. Memory Hellfire 38.4 -> about 26: 21.5 -> 30. Whack 40-45 -> about 31-35: 20-21 -> 26-30. Catch Hellfire and Memory Sweet lose their 3-4 frames of 50 ms | `core/src/mg_gallery.c`, `mg_dodge.c`, `mg_derby.c`, `mg_memory.c`, `mg_whack.c`, `mg_wheel.c`, `mg_draw.c`, `backdrop.c` (SPEED) | L, one game at a time | exact (`test_stage`, kept on and off) | ROM about +1.1 MB a game (30 pictures of 38,400 B), about +4 MB; EWRAM up to 8 B a game; PS1 / DC compiled out |
| **S3-5** | **Kitchen and Fishdex** (S2-4, family 2). `kitchen_draw_back` stands still but for a sparkle and a cloud in the window: bake it per day / night, draw tools, food and HUD live. Recipe book and result card stand still: hold them (S2-1's `ui_modal_full`). Fishdex: hold the settled page, or cache each fish picture (today `fish_draw` + `fx_isqrt` + divisions are 22 ms a frame). Catch card: dim only outside the popping card (S2-2's `ui_dim_under_window_pop`). The simmer banner's shape made before it shows | stations 38.5-42.7 ms -> about 24-27: 20-22 -> 30. Book 48.6 and result card 54.1 -> about 10-15: 17-20 -> 30. Counter / serving 44-45 -> about 30-32 with S3-2: 19 -> 27-30. Fishdex 56 -> about 10: 15 -> 30. Catch card 25.6 -> about 29 | `core/src/cook_gfx.c`, `cook_scene.c`, `fish_gfx.c`, `fish_scene.c`, `backdrop.c` (SPEED) | M | exact; a held book / card freezes its idle motion (pixel-changing, held frames) | ROM about +0.2-0.4 MB |
| **S3-6** | **Dreams** (S2-4, family 3). The sky cannot be kept: it waves every tick. Exact steps: a wave stepper instead of two divisions per column (SP6's `ws_*`: 4.4-6.4 -> about 1 ms); draw each band only where the next does not cover it (about half of the 28-33 ms of rects is overdraw); band rect and `cspan` in one call; the wake-up choice under S2-1's modal hold; the red dim cut to what the card does not cover | choice 80.8 ms -> about 12: 10.6 -> 30 fps. Vignettes 61-71 -> about 42-48: 13 -> 20-22 fps, logic 24 -> 30. Nightmare 3 81 -> about 50. Not 30: see 6 | `core/src/dream_gfx.c`, `dream.c` (SPEED) | M | exact | 0 B |
| **S3-7** | **Cutscene backdrops** (S2-4, family 4). A still layer per panel: baked (SP7's baker) or drawn once into both pages and put back by rect under what moves. Card 2's clouds; the home wallpaper as DMA rows from ROM (`vid_blit` 29 ms -> about 4); fog, eruption and Vapula skies; the bridge's lines | card 2 58.9 ms -> about 20: 15 -> 30. Home 43.5 -> about 18: 20 -> 30. Eruption 54.3 -> about 35: 17 -> 26-30. Vapula 56.9 -> about 32-36: 16 -> 26-30. Fog 92.8 -> about 45-50: 10 -> 20, logic 18 -> 30. After a press 45 -> about 30. P0 bridge 40.3 -> about 28 | `core/src/cutscene.c`, `cutscene_ch*.c`, `cutstage.c`, `backdrop.c`, `gba/src/stage.c` (SPEED) | L, one vignette family at a time | exact where the layer stands still | ROM 38 KB a baked picture |
| **S3-8** | **Library lobby, jukebox, studio STAGE, level select** (S2-4, family 5). Shelves, cabinet and empty stage kept (panel or baked), the record and the cursor live. Level select: title and blurb text as kept panels (outlined text is 6.4 + 3.3 ms a frame); the slide shows the old preview until one new render at its end. Almanac list move: the list as a panel | lobby 41.3 ms -> about 24: 19.8 -> 30. Jukebox 41 -> about 25: 20 -> 30. STAGE 36.3 -> about 24: 22.6 -> 30. Level select 33.3 -> about 27: no more 50 ms frames; the slide 16.9 -> about 26. Almanac list move 25.4 -> 30 with S3-2 | `core/src/lib_common.c`, `lib_jukebox.c`, `lib_almanac.c`, `studio_stage.c` (SPEED); `levels_select.c` (LEVELS area: agree first) | M | exact; the slide is pixel-changing | 0 B (panels) or 38 KB ROM a picture |
| **S3-9** | **Concert** (G2-CO). Setup screen under S2-1's modal hold (the dim is 22 ms). Still scenery (sky, speakers, floor, seats, truss: about 10 ms) kept under the moving parts. The lead's shadowed blit in one ARM pass | setup 60.1 ms -> about 12: 14.5 -> 30. Solo 37.0 -> about 29-31: 22 -> 28-30. Choir 46.9 -> about 39: 15 -> 20-22. Band 56.7 -> about 49: 13 -> 20, logic 23.6 -> 30. Not 30 for band and choir: see 6 | `core/src/concert_gfx.c`, `concert_scene.c`, `gba/src/blit2.c` (SPEED) | M | exact but the setup hold (the stage stands still under it) | UI group +about 300 B of its free slot |
| **S3-10** | **House leftovers** (S2-11). The turn's roof and shell tables built over 2-3 ticks behind the old view, or kept for the four views (they cost 59 ms of the 106). Busy layouts: `draw_region` put back per walker rect, not per row band. House Black Market's item pictures through `vid_blitn` (`vid_blit_c` 6 ms) | turn: one 117 ms frame -> none over 50; house entry 158 + 129 ms -> under 67. Busy Live 32.4 ms -> about 27: 26.8 -> 30. Wave 3 and `home_steady` lose their 4-5 frames. Black Market 33.2 -> 29 ms | `core/src/home_draw.c`, `gba/src/room.c`, `core/src/room_ref.c`, `home_mart.c` (SPEED) | M | the turn is pixel-changing (the old view 2 ticks longer); the rest exact | roof tables in the VRAM gaps: 0 B EWRAM |
| **S3-11** | **Exorcism** (G2-EX). The open steps of EXORCISM_SPEED.md: a layer under a changed one redrawn only inside the dirty box; the tools and true-name windows as retained panels; the verdict and result dims held (15.8 and 7 ms); portrait jobs in idle time only (S3-2) | tools 33.7 ms -> about 27: 28 -> 30. Holy water 36.6 -> about 30. Intro 37.0 -> about 31. Verdict 45.1 -> about 30. Result 43.2 -> about 32. True name 42.5 -> about 33. Prayer 45.8 -> about 38: 19 -> 24. Not 30 in the prayer: see 6 | `core/src/exorcism_scene.c`, `exorcism_gfx.c`, `backdrop.c`, `gba/src/stage.c`, `canvas.c` (SPEED) | M | exact but the held dims | 0 B |
| **S3-12** | **Big text made once.** A wide outlined title is rasterised again whenever its shape leaves the cache: chapter card 4 has 12 frames of 83-87 ms (`text_emit` 23 ms + `ur_run` 11.6 + `em_finish_row` 10 + `out_rect` 9.4), simmer 2 frames of 63-78, every Fun Fair title card and game start 100-201 ms on its first frame. Keep those shapes (pin them while their scene is up; make them under the loading fade) | card 4 22.7 -> 30 fps; simmer's two frames; fair entries 100-201 ms -> about 60-100 | `core/src/ui_raster.c`, `cutscene.c`, `mg.c` (SPEED) | S | exact | GBA shape cache: the scene scratch, 0 B resident |
| **S3-13** | **Menu headroom.** About 30 windows sit at 30-33 ms: shop 32.1, Black Market 32.4 (house 33.2), cafe 30.7, roster 31.0, resident 31, news 32.2-32.6, Almanac list 32.4, items 30.4, Unlocks 30.9, SONG 31.3-32.0, LYRICS 32.1, CHOIR 30.8, swatches 32.0, HomeMart cursor 31.6, pet 28-30. One helper: a list window's body and still text as a retained panel, the cursor row its hole (S2-6 / S2-8). The add-member wizard and the care reaction: the screen under the box held as SP5 holds the world under a dialogue. Credits: card and ticker text as panels. Pet: the new pet picture sliced or prefetched (its 48 ms frames) | menus 30-33 ms -> about 22-26, so a slice, a toast or a recapture no longer costs a frame: news, resident, Unlocks row moves, pet health, pause lose their 4-6 frames of 50 ms. Wizard 34-35 ms -> about 15: 25-27 -> 30. Care reaction 44.5 -> about 22: 19 -> 30. Credits parade 34.3 -> about 30: 26.3 -> 29-30 | `core/src/ui.c`, `ui_draw.c`, `fp_shop.c`, `fp_roster.c`, `fp_resident.c`, `fp_news.c`, `fp_family.c`, `lib_*.c`, `studio*.c`, `credits.c`, `pet*.c` (SPEED; the studio and library parts with S3-8) | L, in slices | exact (`test_panel`); the two holds freeze idle motion | 0 B (panels live in the pages) |
| **S3-14** | **Island hub leftovers.** A tip card is a modal: hold the hub under it (S2-1). Do not build map parts under the town hall menu, or finish them first. The second after a new island's load: 39 frames of 67-117 ms of map parts (after S2-10's fade) | tip card up 19.0 fps, logic 23.8 -> 30. Town hall menu's first seconds 26.2 -> 30 (with S3-2). New island: 39 slow frames -> under the fade | `core/src/fp_island.c`, `fp_island_gfx.c`, `tips.c` (SPEED, after S2-10) | S/M | exact but the hold under the card | 0 B |
| **S3-15** | **PS1: scene-change reads.** Every scene change reads one or two overlay groups: 98-502 ms each, about 150 ms of it waiting for the drive. Read the group a menu is about to open while its cursor rests on the entry (the slot is free until the change), or keep the last group so going back costs nothing | scene changes 260-680 ms -> the fade alone when the guess was right | `ps1/src/overlay.c`, `core/src/scene.c` hook (SPEED) | M | exact | must fit the existing slot and the 72 KB gate: no new resident buffer |

## 5. Suggested order

Three packages a round. Biggest win a player sees first. No two packages of a round share files.

| round | packages | why this round |
|---:|---|---|
| 1 | **S3-1** sim pass, **S3-4** Fun Fair (Gallery first), **S3-5** kitchen and Fishdex | S3-1 is a day's work and clears a hitch from every free-play screen. The Gallery is the worst gameplay number (slow motion). The kitchen is nine windows at 17-22 fps |
| 2 | **S3-2** portraits, **S3-6** dreams, **S3-12** big text | S3-2 removes every 95-325 ms frame on list moves and entries. Dreams run in slow motion. S3-12 is small |
| 3 | **S3-3** creator, **S3-7** cutscenes, **S3-10** house | the creator's held input and tab changes; the story's cards and vignettes; the house turn |
| 4 | **S3-9** concert, **S3-11** exorcism, **S3-8** lobby / jukebox / STAGE / level select | the two old gaps, and the last S2-4 family |
| 5 | **S3-13** menu headroom (in slices), **S3-14** island hub leftovers; **S3-15** PS1 scene-change reads (after PS1-RAM lands) | headroom turns "mostly 30" menus into "always 30"; S3-14 waits for S2-10 |

Shared files to watch: S3-2 and S3-3 both touch `creator_draw.c` and are in the CREATOR stream's area (rounds 2
and 3, never together). S3-2 touches `fp_title.c`, which S2-10 (in flight) also changes: do the title part after
S2-10 lands. S3-4, S3-5 and S3-7 all add families to `backdrop.c` and the baker: small edits, expect a rebase.
S3-8 and S3-13 both touch `lib_*.c` and `studio*.c`: S3-13's library and studio slices come after S3-8. The
owner calls of section 6 (15 Hz skies, walls and auras; sprites for actors) can be decided at any time; each
would become a follow-up to S3-6, S3-9, S3-11, S3-7 and S3-4.

## 6. What cannot reach 30 with the current design

Each of these has a measured gap that the exact steps above do not close. What it would take is listed for the
owner to decide. All are GBA.

| scene | after the package | why | what 30 would take |
|---|---|---|---|
| Dreams (vignettes, nightmare) | about 42-50 ms: 20-22 fps, full-speed logic | a full-screen sky that waves every tick, a scrolling floor and a bubble frame are redrawn whole in Mode 4 | the sky and floor at 15 Hz, kept between steps (the foreground stays at 30); or a simpler sky (straight bands) |
| Concert band and choir | about 39-49 ms: 20-22 fps | flame wall 13-15 ms, beams 8-9 ms, up to 16 scaled chibis | LED wall and beams at 15 Hz, the crowd's back rows as hardware sprites, fewer and wider flame bars (SP6 listed the same three) |
| Exorcism prayer (and true name) | about 33-38 ms: 24-28 fps | the prayer lane moves every tick; the host's aura and the demon's bob redraw big layers | aura, bobs and glints at 15 Hz; the demon as a hardware sprite |
| Fog vignette | about 45-50 ms: 20 fps | drifting fog puffs (ellipses) over the whole screen | fewer or slower puffs, or the fog at 15 Hz |
| Shooting Gallery, Dodge, Whack | about 30-35 ms: 26-30 fps, on the edge | the live actors themselves (targets, clouds, imps, critters) | the actors as hardware sprites (OBJ VRAM is shared with the raycaster cache and the held 3D view, so only outside combat) |
| Creator grid scroll, hat try-on | about 22-24 fps while a direction is held | every step is a new picture and a new panel | the preview and the grid update when the cursor rests (the cursor itself moves at 30) |
| Level select slide | 0.3 s at about 26 fps | a new level's 3D preview per change | accept, or cross-fade two kept previews |
| Credits parade | 29-30 fps, and the far marchers stay dropped | the GBA overworld's sprite governor (29 ms) | a lighter parade ground, or accept |
| Combat (reference, the owner: as it is) | 14.3-18.7 drawn fps at 30 ticks/s; Monster Mayhem 8.8 fps, logic 21.4 | SP8's raycaster | SP8's G-C2 (BG2 2x and the HUD as sprites), not built |

## 7. What was not measured, and why

* **Hell Breach results card, a graveyard with graves, the title as a console boots it:** as in audit 2 (no
  script reaches them in bounded time; the same drawing paths are measured elsewhere).
* **The settle before each window** is not in the tables. Its long frames are in 3.4.
* **Dreamcast per-window drawn fps:** dcrun prints drawn ticks per run. The table has logic ticks/s and the
  ticks over 33.3 ms per window.
* **Real hardware:** emulators only (libmgba, PCSX-Redux, Flycast).

## Files

* Raw logs, tables, profiles and the helper scripts are in the session's evidence folder `evidence/SA3/`
  (not in the repo): `gba/logs/*.log` (every run), `gba/winprof/` (a profile of each missing window's own
  ticks), `gba/winprof60/` and `gba/entry/` (single worst frames and scene entries), `ps1/speed2/`, `ps1/perf/`,
  `ps1/prof/`, `dc/speed2/`, `tools/` (`sa3.py` tables, `winprof.py`, `catprof.py`).
* In the repo, unchanged: the scripts `gba/harness/scripts/speed2_*.txt` and the other tour scripts,
  `tools/speed2.py`, `ps1/harness/speed2.sh`, `dc/harness/speed2.sh`. No tooling fix was needed on this tip.
