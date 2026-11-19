# CI speed report on f3fcfb1 (2026-11-19, run 44525791636, documentation only)

Branch `claude/nice-allen-gr6czm`, commit f3fcfb1 ("CI: drop the 503ing security.ubuntu.com host from apt mirrors", #896).
All jobs green, 18 min 38 s. Machine: the CI runner's libmgba harness (compare only with other CI runs, not with
the 2026-11-18 scratch builds on another machine). No source file was changed by this note.

## What is at 30 and what is not (GBA speed tour, gates off; every scene is above its floor)
| scene | drawn fps | work avg / max ms |
|---|---|---|
| title | 29.7 | 26.0 / 41.7 |
| concert (Nana's solo) | 29.7 | 29.2 / 43.7 |
| concert closed curtain | 29.2 | 12.0 / 76.2 |
| island town hall menu | 26.4 (profile-all run: 30.0) | 26.4 / 77.5 |
| combat first person | 18.9 | 45.0 / 95.0 |
| combat third person | 18.9 | 41.6 / 110.2 |
| every other scene | 29.8-30.0 | 8-30 / under 45 |

* Concert solo is at 29.7 here (the 2026-11-18 scratch build measured 29.5 / 29.7 / 38.8, the 2026-11-15 note 30.0).
  The band window is not in the speed tour; the band is still the open CONCERT-30 item.
* Combat needs to lose 14.6 ms (first person) and 10.8 ms (third person) for 30 fps. Hottest: `r3d_flat_loop`
  (9.6 / 10.2 %), `vid_blit`, `r3d_column_asm` (6.5 %), `r3d_up2` (5.3 %), `plat_view3d`, `ba_face` (4 %),
  `combat_bb_sort` (3.5 %). That is the COMBAT stream's list.
* The title menu opening has 4 slow frames of 55 (worst 58.1 ms). Extra time in `capsule_plain` 6.5 ms,
  `finish_marked` 5.1 ms, `sh_span` 3.3 ms, `scan_transitions` 3.2 ms: the first draw of the menu capsules, not steady cost.
* Island town hall menu: tour 26.4 fps with a 77.5 ms worst tick, profile-all 30.0. Same scene, different run: a
  one-off first-draw spike (`finish_marked`, `capsule_plain`, `scan_crowds`); not yet looked into.
* Invaded: 3 of 295 frames over 33.3 ms (worst 35.8); no single function over 1.1 ms of extra time.

## Music Studio (all tabs and PERFORM)
Every tab holds 30.0 ticks and 30.0 drawn fps. Tightest: lyrics (work avg 31.0, max 32.8 ms: 0.6 ms from the
33.4 ms line), choir 29.0 / 31.9, song idle 26.5 / 28.1. PERFORM sung phase: median 25.4, worst 30.9, 0 ticks over
33.4. Slowest ticks are the line starts (tick 1662, 13 singers, +5.5 ms). PLAY loop over idle: +1.1 ms median.
The Studio is not the problem; lyrics has the least room, so keep text work out of it.

## Hell Breach autopilot (`test-breach2`, 7 levels x 3 windows, gates off): all 21 windows MISS
Drawn fps 3.8 to 16.6, work avg 41 to 139 ms, worst frame 84 to 1423 ms, 47 to 76 frames over 33 ms per window.
| level | start room | middle | late (drawn fps) |
|---|---|---|---|
| cathedral | 4.7 | 7.8 | 11.8 |
| cloud nine | 3.8 | 3.9 | 5.2 |
| factory | 5.5 | 4.8 | 6.6 |
| graveyard | 5.2 | 10.4 | 5.2 |
| sewers | 6.3 | 5.9 | 10.8 |
| underbelly | 6.6 | 6.6 | 8.9 |
| vape cellar | 8.6 | 8.7 | 16.6 |
* Cloud Nine is the slowest level (work avg 126-139 ms, worst frame 1.4 s). The start-room windows include a load
  of 230-315 ms (the "load ms" column), which is one long tick, not the cause of the low fps.
* Logic stays at 8-30 ticks/s: below 30 in nearly every window, so the autopilot's game clock also runs slow here.
* These are the combat numbers again (the same `r3d_*` code, bigger maps). The floors are 85 % of the first
  measurement and only gate with `BREACH2_GATES=1`, so CI stays green while the table says MISS.

## PS1 disc: the margin fell
`_end` 0x801ed490, **76,640 B below the stack top** (0x801FFFF0). CLAUDE.md's rule is 72 KB (73,728 B): met, with
2,912 B to spare. The last recorded numbers were 110,352 B (CONCERT-30, 2026-11-15) and 132,608 B (tip, 2026-11-15),
so about 33.7 KB of resident growth landed since. Where it sits now (resident .text 741,528 B, .rodata 545,432 B,
.bss 478,936 B, overlay slot 131,072 B):
* biggest objects: `packed_art` 193,658 B, `ps1/src/gfx` 144,117 B (`s_prim` 108,544 B), `roster_data` 137,698 B,
  `core/src/town` 95,968 B (`doomo_town` 89,504 B), `ui_raster` 60,696 B, the portrait files 44 + 43 + 40 + 33 KB.
* The overlay groups `studio` (172,032 B) and `home` (170,744 B) are the largest, loading over `.borrow`.
* Not found yet: which commit took the 33.7 KB. First thing to do is `ram_ledger.py` on the tip and on the last tip
  with a 110 KB margin and diff the two; the `.bss` and `roster_data` are the first suspects (GBA-500 and the
  roster / town tables grew the other ports' tables too).
Anything that adds resident PS1 bytes should stop until this is explained: one more 3 KB package breaks the rule.

## Next steps (in order)
1. PS1 margin: bisect the 33.7 KB (above). Owner: the lead of the stream that owns RAM (MEM2 in SPEED).
2. COMBAT: send "combat needs -14.6 / -10.8 ms and the 21 breach windows" to the COMBAT lead (speed audit 4 list).
3. CONCERT-30: band-only baseline, then guitars and drums through `dream_ellipses` (see CONCERT-30-findings.md).
4. Title menu and town hall menu first-draw spikes: spread the capsule bake over two ticks.
