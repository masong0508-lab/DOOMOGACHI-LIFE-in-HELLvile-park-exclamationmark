# Life sim on the PS1: status checklist

Written 2026-11-02 for the PS1-first focus (ROADMAP.md "Current focus"). This is a read of the repo, not a new
measurement: nothing here was run on PCSX-Redux, DuckStation or a console while writing it. The native column
is `make -C core test` (677 tests, 19 journeys, all passing on this tree). Update the tables when a PS1 harness
run changes a row, and keep the "Last PS1 run" line honest.

Last PS1 run recorded in docs/PS1.md: the life tour (section 12), taken **before** LS1. ROADMAP "Tiny" item 2
(re-run the PS1 harness after LS1) is still open, so every fps number below is stale until it is done.

Status words: **Verified** = a PS1 harness result is written down in docs/PS1.md. **Native only** = passes in the
native runner, never driven on the PS1. **Written** = code exists, no run of any kind is recorded. **Missing** =
not built.

## 1. Boot, memory and speed

| Item | Status | Notes |
|---|---|---|
| Disc links and boots | Verified (H0) | re-check after the branches land |
| Life tour fps (docs/PS1.md "Life tour") | Verified, stale | hub 25.0, Town Hall menu 25.6, news / Citizens / a citizen / Towers 30.2; island hub and its menu are the only screens under 30 |
| RAM headroom with the life sim + LIFE chunk | Written | the overlay budget was last recorded at 123,632 B free (H-series); nothing records the number after F1/F4/F6 |
| Re-run of the PS1 harness after LS1 | Missing | ROADMAP Tiny 2; run `ps1/harness/perfall.sh DIR life` and `game_smoke.txt` first |

## 2. Saves on the memory card

| Item | Status | Notes |
|---|---|---|
| Story and free-play saves (`BASLUS-DOOMO0` / `1`) | Verified | header, CRC, safe second-copy write; docs/PS1.md section 9 |
| Machine config file (U1p, `BASLUS-DOOMOC`) | Written | needs one PS1 harness run to confirm (ROADMAP section 0) |
| Quick save cost (U3b story, U3c island) | Written | see below |
| Family / LIFE / LOSS chunks round trip | Native only | `test_save.c`; no PS1 `save` / `load` run with a family in it |

Quick save on the PS1 blocks the game loop for about one second per 8 KB block (docs/PS1.md section 9), and the
platform draws the "Saving..." screen over it. From `make -C core test` sizes (+ about 0.6 KB card header):
a story save at its largest (13 KB) is 2 blocks, about 2 s; an untouched Classic island (7 KB) is 1 block, about
1 s; a heavily edited island (59-66 KB) is 8-9 blocks, about 8-9 s. START, START is two presses but not an instant
action on this console. If that feels bad, U3b/U3c can show a longer toast or ask once for big islands; decide
after a real PS1 run, not before.

## 3. Life sim features on the PS1

| Feature | Native | PS1 |
|---|---|---|
| New Island screen -> hub (5 presses) | Verified (J9, life_new_island) | Verified by `perf_life.txt` (reaches the hub; no press budget run) |
| News, Town Hall menu, Citizens, a citizen, Towers | Verified | Verified (fps only) |
| Help a citizen, missing item, feed, introduce, rent | Verified (journeys) | Missing: no PS1 script drives them |
| The Doomogachi pet screen | Verified (`life_pet`) | Verified (`perf_pet.txt`, fps only) |
| Exorcism, Dream, graveyard | Native only | exorcism has `perf_exorcism.txt`; graveyard has no PS1 script |
| Island START menu + quick save (U3c) | Verified (J7) | Script ready (J7 on the disc, not yet run) |
| Story pause + quick save (U3b) | Verified (J6) | Script ready (J6 on the disc, not yet run) |
| Family table, ageing, births, loss, pets' needs, resident deaths, memorial flowers (F1/F4/F5/F6 core) | Native only | Written; no UI yet (F2) so a PS1 run has nothing to press |
| House (H3 `room.c`) | Native only | Documented as built and measured (docs/PS1.md section 7, `harness/scripts/home.txt`); ROADMAP reconciled 2026-11-02; not re-run since LS1 |
| Mature disc variant (E9), language tables on disc (L6) | Missing | later waves |

## 4. Suggested PS1-first order

1. Run `game_smoke.txt` and `perf_life.txt` on the current tree; record the numbers in docs/PS1.md section 12 and
   in section 1 above (closes ROADMAP Tiny 2, confirms U1p).
2. Add a PS1 script for the press journeys that already pass native (J6, J7, J9, rent, feed) by reusing the native
   journey files with the harness `--script`, so press counts are checked on the disc too.
3. Time a real quick save on the PS1 (U3b/U3c) and the first family save once F2 has a UI.
4. Only then take new life-sim packages, in the ROADMAP wave order, with a PS1 row added to this file each time.

Step 2 started (2026-11-02): `ps1/harness/run.py` now understands `journey-begin` / `journey-end`, so the native journey
files play on the disc unchanged and fail before the emulator starts if the presses go over `budgets.tsv`. The game
`harness` target runs J6 and J7. Still to do: run them on PCSX-Redux (nothing here was run on the emulator), then
J9, rent, feed (they need the boot flow or typed names, which the PS1 harness has not been checked with).

Step 2 widened (2026-11-02, patch doomo-ps1-journeys-all-nov02m): the PS1 mailbox now also runs `item`, `expect-item`, `problem`,
`expect-problem`, `expect-rel`, `expect-money`, `lang` and `expect-lang`, and `run.py` checks `expect-presses`. The game
`harness` target plays 15 core journeys on the disc (j2, j4, j6, j7, j8, j10, j11 and the eight `life_*`). Left out: j1, j3,
j5, j9, which start at BOOT. Written and checked on the host only (hcmd.c syntax-checked, every journey compiles in run.py);
nothing has run on PCSX-Redux, so the Rent / Feed / Introduce / Help rows stay "Missing" until that run passes.
