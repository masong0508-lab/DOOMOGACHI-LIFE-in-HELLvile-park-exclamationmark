# PERFORM fixes (Music Studio, GBA): the sung-phase step

Written 2026-11-07 (nov07g). Companion to `docs/MUSIC_STUDIO_ROADMAP.md` ("Facts to know before MS-P4") and `docs/STUDIO.md`.
**None of these is built.** The gains are ablation figures from the GBA harness (nov07d tree), not results of the fixes themselves.

## The problem in three lines

* PERFORM is ~49-50 ms in the instrumental intro and **~58 ms once the first line is sung** (13 singers on the "Park United!" demo).
* The intro is just under the 3-vblank cliff (50.2 ms). Past it a tick takes 4 vblanks: **logic 30 -> ~22 ticks/s, the song plays ~25% slower** while people sing.
* Goal of these fixes: sung phase back under 50 ms (tempo restored), without changing a pixel. Reaching 30 fps still needs MS-P4 (chibi tile cache).

## The fixes (do in this order; each is independent)

| # | Fix | Where | Saves (measured by switching it off) | Exact? |
|---|---|---|---|---|
| F1 | **Karaoke bar as a retained panel / shape image.** The bar (`plat_shade_rect` + `ui_frame`) is redrawn every frame and costs 2.4 ms even in the intro. Key it on position, width and the lyric-free state; draw the text over it. Use the G7 retained panel (`gba/src/panel.c`; text lies on the body) or a shape image like the title logo. | `karaoke()` in `studio_stage.c` | **~2.4 ms, every PERFORM phase** | yes, if the shade is part of the kept body; check by `cmp` |
| F2 | **Beat hearts without 15 heart rasters.** Every 16 ticks `draw_wall` lights the LEDs with `beat && d < 3` and each is a `ui_heart_flat` (`hf_row`, `em_finish_row`): a +5..7 ms spike. Pre-bake one 7x6 heart as a `DoomoImage` and blit it, or draw the lit LEDs as the same 3 rects the unlit dots use plus a colour. | `draw_wall()` in `studio_stage.c` | removes all intro spikes (7 of 37 frames -> 0); median unchanged | image path: exact if the baked heart equals `ui_heart_flat(x, y, 3, ...)`; a different shape is a visual change, ask first |
| F3 | **Karaoke text as runs.** One `ui_text` per glyph with a per-glyph colour and bounce. Draw one `plat_text` per same-colour run, as `draw_syllables` does on the LYRICS tab (nov07c; plain font style, English; other styles keep the old path). The bounce (`song_beat_age() < 4`) lifts one syllable: keep it a run of its own. | `karaoke()` | ~1.5 ms | yes (LYRICS did this byte-identically) |
| F4 | **Follow spot with fewer rects.** `spot()` draws a checker row every 2 px from the stage top to the lead's feet (~40 `plat_checker` rows a frame). The cone is linear: draw it as 5-6 stacked trapezoid steps (wider checker blocks) or a baked shape image keyed on (x0, x1, top, bottom). | `spot()` | ~1.3 ms | no, unless the steps are 2 px rows exactly; a coarser cone is a visual change, ask first |
| F5 | **Note glyph without an ellipse.** `st_note_glyph` uses `ui_fill_ellipse` (-> `ellipse_draw` -> `fx_isqrt`, new in the sung phase) for a 5x3 head. Draw the head as the 3 rects the ellipse produces (read `ellipse_draw` for r=(2,1) to keep it exact). | `st_note_glyph()` in `studio.c`; also `draw_mouth` costs ~1.1 ms together with the notes of 13 singers | ~0.5 ms | yes if the rects equal the ellipse rows |
| F6 | (small) **Voice tuning memo** = MS-P1. The tuning search is not the steady cost (median unchanged) but it is the +3..6 ms **onset spikes** (26 -> 17 spiky frames per 100 without it) and the 69 ms frame at the first onset. Cache by (voice hash, MIDI note), 64 entries, bit-identical output. | `tune_voice()` in `song_player.c` | spikes only | yes (test compares cached vs uncached blips) |

Rough total F1+F3+F4+F5: about -5.7 ms, so ~58 -> ~52 ms. That is **not** enough alone: the intro would be ~46 ms but the sung phase still sits above 50.2 ms.
To clear the cliff for certain, add one of: F2's heart image *and* a cheaper wall (checker rows 100 -> <60), or part of MS-P4 (the 13 singing chibis).
Be honest about it in the commit message: the first batch restores the tempo only if the sum lands under 50 ms on the ROM. Measure, do not assume.

## How to check each fix

1. **Exactness (native, both sizes):** the 98-shot script in `docs/MUSIC_STUDIO_ROADMAP.md` ("How to check a studio drawing change is exact") plus a PERFORM version: tab to the stage (`press R`; `down SELECT / press R / release` x3; `press R`), `press A`, then `shot` every 9 ticks for ~300 ticks so the intro, the first line and several syllables are covered. `cmp` old vs new at 240x160 and 320x240. Anything that is not byte-identical is a visual change.
2. **Cost (GBA):** `make -C gba profile-studio` does not cover the sung phase (its PERFORM windows are the intro). Use a script that presses A, `wait 40`, `fps all`, `wait 330`, `fps end` with `--worklog`: one `work TICK ms` line per drawn tick inside the window. Gate on: **sung steady median < 50 ms, logic >= 29.5 ticks/s, intro spikes 0**.
3. `make -C core test` stays green.

## Not in this list

* The chibis (~14 ms on CHOIR, the largest share of PERFORM): MS-P4.
* PS1: unmeasured. The 3-vblank cliff is a GBA (59.7 Hz / 50.2 ms) figure; check where the PS1 equivalent sits before assuming the same fix order.
