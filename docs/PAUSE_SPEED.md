# Pause screen speed on the GBA (sub-roadmap)

Goal: raise the overworld pause panel's drawn fps (baseline below) without
breaking the EWRAM ledger, the fps floors or the parity tests.

## Baseline (libmgba harness, `make -C gba profile-pause`, 2026-11-06)

| scene | logic ticks/s | drawn fps | work avg / max |
|---|---:|---:|---|
| visit under the pause | 29.0 | 23.6 | 40.1 / 121.4 ms |
| pause | 26.4 | 13.9 | 67.4 / 120.7 ms |

Pause profile: `ow_spr_rows` 18.5%, `vid_rect` 7.5%, `vid_shade_words` 7.3%,
`plat_overworld` 6.9%, `ow_tile16` 5.6%, `vid_blit` 4.0%, `vid_shade` 3.5%.

## Why

`plat_hold_frame` (plat_ext.h, U4) is implemented natively only. On the GBA it
is the weak default (returns 0), so `story_draw` rebuilds the whole 3D world
every paused frame and then dims the full screen outside the window
(`owfx_draw_pause` -> `ui_dim_under_window_pop`).

## Steps (each lands alone, each keeps `make test` and `make -C gba test-speed` green)

- **P0** this document and the measured baseline. (done)
- **P1** GBA `plat_hold_frame`, Mode 4 only: after a Mode 4 frame is shown,
  keep it as the backdrop; return 0 on tile (Mode 0) frames and skipped
  ticks so the core falls back to today's path. No new EWRAM: use the two
  VRAM pages (the retained-panel code in `panel.c` already reasons about
  front/back page contents).
  *Plumbing landed (2026-11-06, `gba/src/hold.c`, not built here):* the hold
  state, the eligibility rules (last flip showed a Mode 4 page, not a skipped
  tick, a tile-frame flip releases it) and the `hold_flipped` hook in
  `video_flip`. It still answers 0 (`HOLD_RESTORE_READY` 0), so behaviour and
  the fps gates are unchanged. **Why the restore is not in P1:** both Mode 4
  pages are needed to flip, so there is no clean third copy of the backdrop,
  and the core dims and shades the window's shadow over it on every paused
  frame (that compounds on a page that is not restored). The restore needs
  P2 (dim the held pages once) and P3 (keep the ring around the window:
  shadow, title pill, corners; the 2.5 KB of VRAM at 0x06013600..0x06013FFF is
  free in Mode 4) together; flip `HOLD_RESTORE_READY` when they land.
- **P2** dim once: apply the 8-level dim to the held backdrop when the pop-in
  ends (`ow.pause_t` >= 12), not every frame; per frame repaint only the
  window, its drop shadow strip and the title pill margin.
- **P3** restore list: the shadow / title-pill / sparkle pixels outside the
  window that the core redraws over a dimmed backdrop (see `ui_window_shadow`,
  `ui_title`, `ui_sparkle`) are put back from the held copy before drawing.
- **P4** measure: update `docs/GBA.md` pause rows and the EWRAM ledger.
  Target: pause drawn fps >= the visit's (about 23) and EWRAM free unchanged.
  *Checked here 2026-11-06 (no arm-none-eabi-gcc, so no ROM and no fps):* see
  "P4 status" below.

## Acceptance

`make test`, `make -C gba test` (parity), `make -C gba profile-pause` before /
after, `tools/ram_ledger.sh` unchanged. Needs arm-none-eabi-gcc and libmgba.

## P2 + P3 landed (2026-11-06, not built or measured here: no arm-none-eabi-gcc)

Design, kept small so it needs no new RAM:
- The early hold (`plat_hold_frame(1)` at open) stays 0 on the GBA: the pop-in
  frames (`pause_t` < 12) redraw the world and dim as before.
- Once the pop-in is over, the first full frame (world + dim + window + shadows +
  title pill) is shown. The next tick calls `plat_hold_frame(2)`: `gba/src/hold.c`
  DMA-copies the shown Mode 4 page into the other page, so both pages carry the
  settled frame (P2: the dim happens once, in that frame).
- From then on `story_draw` calls `owfx_draw_pause_ex(1)`: no world, no dim; the
  window and the title pill repaint, and `ui_shade_clip` cuts every UI shade rect
  (the window's and the pill's drop shadows) to the window rect, so nothing
  outside it is shaded twice (P3: the shadow ring and pill margin simply stay in
  the pages; no restore list is needed because nothing outside the window/pill
  changes).
- Fallback to the full redraw (and it stays off until the menu closes) when a
  toast is pending, a dialogue or modal is up, the screen flashes or fades, or
  the platform reports the hold lost (`plat_hold_frame(4)`: a tile frame flip).
- `HOLD_RESTORE_READY` stays 0: it only gates the early hold.
Still to do on hardware/mGBA: `make -C gba profile-pause`, parity, ram_ledger (P4).

## P4 status (2026-11-06)

Checked without a GBA toolchain (host only):
- `make -C core test`: 871 passed, 0 failed, with P2+P3 in the tree.
- RAM: P2+P3 add 2 bytes of statics in `gba/src/hold.c` (`last_page`,
  `held_page`) and the `ui_shade_clip` rect in `core/src/ui_raster.c` (a few
  bytes). No buffer, no new pool: the other Mode 4 page is the copy. The
  largest statics (`tools/ram_ledger.sh`, host sizes) are unchanged:
  `buf.0` 104704, `doomo_town` 89504, `doomo_map` 32784.

NOT measured (needs arm-none-eabi-gcc + libmgba; no number below is invented):
1. `make -C gba profile-pause` and compare with the P0 baseline (pause 13.9
   drawn fps, 67.4 / 120.7 ms; visit 23.6 fps).
2. `make -C gba test` (parity: the pause shot must match the full redraw).
3. `make -C gba test-speed SPEED_BASE=<merge base>`, then copy the new
   `overworld pause` row into `docs/GBA.md` (rows near the G2 / G5 tables) and
   `sh tools/ram_ledger.sh` from the GBA link map for the EWRAM ledger.
Pass: pause drawn fps >= the visit's (about 23) and EWRAM free unchanged. If
the pause falls short, the next cost to look at is the window repaint itself
(`ui_window_pop`) and `ow_spr_rows` (18.5% of the old pause profile).
