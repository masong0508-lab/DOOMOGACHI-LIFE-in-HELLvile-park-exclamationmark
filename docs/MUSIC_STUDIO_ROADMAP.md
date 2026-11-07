# Music Studio roadmap: functionality, fps, fun

Started 2026-11-06. Companion to `docs/STUDIO.md` (what is built) and `ROADMAP.md` (the game). Package IDs are `MS-*`; sizes are S / M / L.
This is a first version: every package has a measurable gate, and the first one (MS-P0) exists to replace the guesses below with device numbers.

## 1. Where the studio stands

Built (all host-tested): 4-track tracker (32 steps x patterns A..D, order list of 16), 8 lyric lines with live syllable fit, a 25-citizen choir with
sections, PERFORM show, a 6-page song book (`'SONG'` save chunk), 14 templates, a deterministic composer, "Inspire me!", undo, 16 unlockables.
~3,100 lines in `studio*.c` plus the shared player `song_player.c` (960 lines) and `song_book.c`.

### What it costs (measured in this session unless marked)

Native runner, `stats` after each step of `core/tests/scripts/studio.txt` (draw calls per frame; these are the platform's work, not device time):

| screen | rects 320x240 | rects 240x160 | other |
|---|---:|---:|---|
| SONG (idle / playing) | 713 / 758 | 390 / 389 | 11-13 images |
| NOTES (tracker) | 760 | 536-544 | text 75-79 |
| ORDER | 646 | 492 | |
| **LYRICS** | 566 | 438 | **text 213** (every other tab: 25-79) |
| **CHOIR / picker** | 797 / 839 | 653 / 689 | **images 37-39** (the chibis), shade 9-21 |
| PERFORM (sing / choir / bows) | 743 / 801 / 804 | 382 / 440 / 443 | **checker rows 150 / 193 / 203** (320) and 96 / 121 / 128 (240) |
| FILE / templates | 597 / 581 | 446 / 453 | |

Device numbers already in the docs (not re-measured here):

| | PS1 (PCSX-Redux) | GBA (mGBA) |
|---|---|---|
| studio tour | **30 fps** (`docs/PS1.md` speed pass 3) | **27.8 fps / 34.1 ms** work per tick, 0.8 ms over the 33.3 ms budget (`docs/GBA.md` speed pass 3) |
| for scale | island hub 22.0 (2,030 rects), creator 25.7 (2,730), concert band 19.9 (4,600 rects + 247 checker rows) | |
| PS1 overlay | studio group 45,524 B of the 126,976 B slot | |
| RAM | `.bss` 5,364 B book + 2,002 B working set; GBA EWRAM is already ~28 KB over in the live tree (`docs/design/INTEGRATION_TODO.md`) | |

**Not measured anywhere:** the PERFORM show on PS1 and GBA (the tour's 30 fps is the editor tabs), and the cost of the per-syllable voice tuning.
Both are the most likely places for real dips, so they come first.

### What the code leaves on the table

* The tone player drives `plat_tone` channels 0-3 only. The PS1 has 24 SPU voices and an **unused reverb work area** (`docs/PS1.md`, SPU map:
  `0x7e000-0x7ffff`); the new music engine's `plat_voice` / ADSR layer is not used by the studio either.
* Every syllable of a sung note runs a voice-tuning search of up to ~130 `audio_voice_freq` calls (`docs/STUDIO.md`, Singing). That is a frame spike per syllable, and the same (singer, note) pair recurs constantly.
* The studio has undo but no redo, no per-track mute / solo, no metronome, 4 patterns, 6 song pages.
* Songs live only in the studio and the Concert Hall: the Library jukebox and the music engine never play them (`grep song_book` in `lib_jukebox.c` / `mus_*.c`: nothing).
* A performance pays a flat +6..+18 happiness no matter who is listening, although the roster has favourite genres (Inspire me! already reads them).

## 2. Goals and rules

1. **FPS:** every studio tab and PERFORM hold 30 fps on PS1 and GBA at 240x160 (GBA: work per tick <= 33.3 ms with margin, aim <= 30 ms). No frame spike on a syllable.
2. **Functionality:** the usual editing gaps closed (redo, mute / solo, metronome, range copy, more patterns) without breaking saved songs.
3. **Fun:** songs matter to the town: citizens have taste, songs can be requested, shared with the jukebox, and sound much better on PS1.

Rules every package keeps: the save format stays append-only (old saves load; a damaged chunk resets); `studio_compose(genre, seed)` stays deterministic
(`core/tests/data/music_golden.h`); `make -C core test` stays green; no malloc, no floats; GBA EWRAM and the PS1 64 KB stack reserve are budgets, not suggestions
(a package that adds `.bss` says how much and what it moves to `scene_scratch()`).

## 3. Packages

### A. Measure first

> **MS-P0 status (2026-11-07): the GBA half is done.** `make -C gba profile-studio` (and a CI step) now measures every tab and PERFORM;
> the table is in `docs/STUDIO.md` "GBA speed (MS-P0)". Still open: the PS1 `perf_studio.txt`, the `stats` regression lines in
> `core/tests/scripts/studio.txt`, and the studio stop in the GBA fps tour. **What the GBA numbers change below:** LYRICS 60 ms (12 fps),
> CHOIR 52 ms (15 fps) and PERFORM 56 ms (13 fps) are the real gaps, not the editor tabs (31-36 ms). PERFORM is chibi-bound
> (`vid_blit` + `res_get_look` + `cache_get` about 35%, the checker wall about 2%), so **MS-P4 (chibi tile cache) pays for CHOIR and PERFORM
> together and should go before MS-P5's wall bake**; LYRICS is 28% per-frame song derivation, so **MS-P3 should cache the syllable split / singer
> lookups per line, not only batch the text**; no single-syllable spike showed up in what was believed to be the sung window (it was the instrumental intro: see "Facts to know before MS-P4"; onset spikes are real, +3..6 ms, ~9 of 26 spiky frames per 100), so **MS-P1 (tuning cache) is a small win, not nothing**. Order to take them on GBA: MS-P4, MS-P3, MS-P2, MS-P1, MS-P5.
> **MS-P3 step 1 landed 2026-11-07** (syllable-split memo + line-part memo, exact): LYRICS 60.4 -> 45.3 ms, 12.4 -> 20.2 fps, logic back to 30.1 ticks/s. Left in MS-P3: batch the per-glyph text of `draw_syllables`. MS-P4 (chibis: CHOIR 52 ms, PERFORM 56 ms) is next.
> **2026-11-07 later:** the `res_get_look` share was the `blh` look-hash memo thrashing (fixed: CHOIR 50.8, PERFORM 52.1 ms); `draw_syllables` now draws glyph runs (LYRICS 38.6 ms, 20.3 fps, shot byte-identical). Remaining for CHOIR / PERFORM: `vid_blit` of 25 chibis a frame (15-21%) and `cache_get` / `bodyspr_image` (8-10%): a per-singer tile cached as one image, redrawn only when the singer, section or selection changes.
> **2026-11-07 (nov07m): `vid_blit` is ARM** (scale 1 without flip; `docs/STUDIO_30FPS_PLAN.md` section 7): the sung PERFORM median 47.0 -> 40.5 ms (worst 52.9 -> 46.4, no tick over 50.2), CHOIR 46.5 -> 42.3, PERFORM intro 42.4 -> 36.0; exact (in-ROM test against the old C, 16-scene frame parity, 150 sung frames identical). Left to 33.3 ms: about 7 ms, i.e. plan option B (stage image memo) next.

> **2026-11-07 (nov07d) MS-P2 first step landed: the shared studio frame.** Measured by switching parts of the frame off on the GBA harness:
> the backdrop + tab strip + hint bar are **~14 ms of every tab** (about 16 ms of the 33 ms an empty tab costs; the tab strip alone ~5 ms,
> the rest is `ui_backdrop`'s bands, ~60 polka-dot rects, hearts, stars). Most of the backdrop lies under the opaque content panel. Done:
> `ui_backdrop_hole` + `studio.c backdrop_hole()` skip everything wholly inside the panel's body (exact; native shots identical, see
> `docs/STUDIO.md`). With it: SONG idle 35.2 -> 33.1, NOTES idle 33.4 -> 29.6, ORDER 31.0 -> 27.1, LYRICS 38.6 -> 36.5 ms. **SONG playing
> (34.4), LYRICS, CHOIR (46.9) and PERFORM (51.0) are still over 33.3 ms.** The next real step is **MS-P4** (below, with the facts you need).
> Not done from MS-P2: the tab strip (~5 ms, redrawn every frame; a retained panel / shape image is the lever), the tracker's per-cell rects.

### Facts to know before MS-P4 (chibi tile cache), measured 2026-11-07

* **Cost model (GBA):** one `plat_rect` / `out_rect` is roughly **20 us**, so the *number* of rects matters more than their area. A full-screen
  studio frame is ~400-540 rects at 240x160 (table in section 1).
* **The chibis are ~14 ms of CHOIR.** Drawing CHOIR with the 25 `st_chibi` calls removed: 50.8 -> 36.8 ms (22.5 fps). So after chibis are free
  CHOIR is *still* ~37 ms: it needs the shared-frame savings (MS-P2) **and** fewer tile rects (`ui_round_rect` x 2 and a frame + badge per
  singer = ~100 rects) to reach 30 ms. MS-P4 alone will not make CHOIR 30 fps.
* **Where a chibi's time goes:** `vid_blit` (scale 1, no flip: the fast "cached chibi" path in `gba/src/video.iwram.c`, halfword reads when the
  source is 2-aligned; the cache `pool` is EWRAM, a 16-bit bus), `cache_get` + `bodyspr_image` (look hash, `blh` lookup, `memo`), `memcmp` of the 48 gene
  bytes per call (1.6%). `bodyspr_image` is called once per chibi per frame; the CHOIR tab draws 25, PERFORM 25 + conductor + crowd silhouettes.
* **Already done on this path:** `blh` fully associative (40 slots) with a per-id hint slot (nov07d), `memo` 128 entries (nov07d, +128 B EWRAM).
  A hit is cheap now; remaining cost is per-call overhead x 25 and the blit itself.
* **Pixels vs calls:** a 16x16 chibi is 256 px; `vid_blit` is ~15% of CHOIR (~7.5 ms for 25 = 0.3 ms each). A pre-composed tile (frame + chibi + badge
  as ONE `DoomoImage`, redrawn only when singer / section / selection / bop frame changes) turns ~4 rect calls + 1 blit per singer into 1 blit.
  Tile cache key must include: singer id, section, `sel`, bop (0/1), chibi frame, look hash (`bodyspr_image` already keys on it). Memory: a 25-tile
  cache is 25 x tw x th bytes (~25 x 24 x 31 = ~19 KB at 240x160): **EWRAM has ~20.5 KB free**, so the tile must live in the portrait `pool`
  (18,432 B, 16x16 = 1 block each; `PC_BLOCKS` / `CacheEnt`) or in `scene_scratch()`, never new `.bss`. Decide this first.
* **Retained panels (G7, `gba/src/panel.c`)** already keep a panel's body between frames (a DMA per row from the front page); text / images /
  checkers lie on it. 8 panels, 80-rect pool. The studio's content panel could use it, but anything drawn *over* the body every frame (all of the
  tabs) must stay out of the key, as the title and HUD work did. This is the other big lever for SONG / NOTES / ORDER.
* **PERFORM's 74-88 ms climb is explained (nov07d tree, GBA harness, per-tick `--worklog` over a whole show).** The show has two phases.
  The first ~4.5 s (`show_t` 30..~170) is an *instrumental intro* (the lyric bar shows the music-note placeholder); the climb is the **first sung
  line** ("1st half (13)": 13 singers). Steady work goes **~50 -> ~58 ms** and a one-frame 69 ms spike lands on the first onset. Why it matters
  more than 8 ms suggests: the intro sits at 49-50 ms, right under the **3-vblank cliff (50.2 ms)**. Past it, a tick takes 4 vblanks, so
  **logic falls from 30 to ~22 ticks/s and the song plays about 25% slower while someone is singing** (the tempo follows `song_tick` per tick).
  Fix the draw cost and the tempo comes back before fps does; "PERFORM later" figures in `docs/STUDIO.md` are the pre-first-line intro, not the sung phase.
  What the sung phase adds (ablation, steady medians, ms: intro 49.3, sung 58.4):
  karaoke bar **-4.9** (of which the empty bar alone, `plat_shade_rect` + `ui_frame`, is **2.4 even in the intro**; the per-glyph `ui_text` and
  part label are ~2.5), follow spot (`spot`, ~40 `plat_checker` rows + star) **-1.3**, mouths + floating notes (13 singers: `draw_mouth`, `st_note_glyph`
  with `ui_fill_ellipse` -> `fx_isqrt`) **-1.1**. Switching the voice tuning search off changes the median by 0.2 ms but cuts the spiky frames
  26 -> 17 of 100 (it is the +3..6 ms onset spikes; `res_get_voice` 1.5% is `choir_double` / section lookups per syllable, switching it off: no change).
  Native draw counts agree: rects 385 -> 466, text calls 3 -> 28, checker rows 100 -> 133 (240x160) at the first sung line.
  **Separate, in the intro: every 16 ticks a +5..7 ms spike is the beat-lit LED hearts** (`draw_wall`: `lit = ... || (beat && d < 3)`, each a
  `ui_heart_flat`, `hf_row` + `em_finish_row`): switching `beat && d < 3` off removes all of them (7 of 37 frames -> 0) and changes nothing else.
  Audio output is not a factor (making every `plat_tone` call a no-op: no change). The earlier note that the "sung window showed no spike" was wrong:
  that window (`perform early/later`) was the intro. **Cheapest PERFORM fixes, in order:** (1) bake the karaoke bar (shade + frame) as a retained panel /
  shape image: -2.4 ms in every phase; (2) draw the beat hearts as one pre-baked heart image or fewer / flat dots: removes the spikes;
  (3) per-run `plat_text` for the karaoke line (the LYRICS tab already does this, nov07c): ~-1.5; (4) the follow spot as 4-6 rects, not 40 checker rows: ~-1;
  (5) a note glyph without `ui_fill_ellipse`: ~-0.5. Together ~-6 ms puts the sung phase near 52 ms, back under the cliff only with (1)+(2)+(3)+(4);
  the structural fix is still the chibi tile cache (MS-P4).
* **Silhouettes are slower than chibis:** `vid_blit` with `IMG_SILHOUETTE` skips the halfword fast path. A pre-baked silhouette image per crowd id
  would be a cheap win for PERFORM.

The PERFORM fix list (karaoke bar, beat hearts, follow spot, note glyph, tuning memo) with gates is in `docs/STUDIO_PERFORM_FIXES.md`.

### How to check a studio drawing change is exact (use this before and after MS-P4)

1. Native, both sizes, every tab over time (this is what proved nov07d): a script `scene TITLE / wait 2 / push STUDIO -107 / wait 20 / expect-scene STUDIO`,
   then for each tab 14 x (`shot NAME_NN`, `wait 9`) and `press R`, run with `core/build/runner --quiet --script S --shots DIR --saves DIR/s --w 240 --h 160`
   (and 320x240) on the old and the new tree, `cmp` every PNG. 98 shots a size: all identical for nov07d.
2. GBA harness `shot`s are **not** a reliable exact check when a change alters frame time: the animation tick a shot lands on moves (nov07d: 3 of 10
   differed, all pixel noise from drifting hearts / notes). Use `sync on` scripts or the native check above.
3. Then `make -C gba profile-studio` for the numbers, and `make -C core test`.

| ID | Size | What | Gate |
|---|---|---|---|
| **MS-P0** | S | Add `stats` lines to `core/tests/scripts/studio.txt` (the table above, as a regression log). Add `ps1/harness/scripts/perf_studio.txt` (every tab + a full PERFORM show; the other scenes have `perf_*.txt`, the studio has none) and a studio stop in the GBA fps tour. Run the GBA slow-frame report (`--worklog`, `docs/CI_SLOWFRAMES.md`) over a sung line to see the syllable spike. | A table of fps / ms per tab and PERFORM on PS1 and GBA, pasted into `docs/STUDIO.md` Budgets; the top 5 functions of the worst tab and of PERFORM. |

### B. FPS (do these in this order; each is gated by MS-P0's numbers)

| ID | Size | What | Gate |
|---|---|---|---|
| **MS-P1** | M | **Syllable tuning cache.** Memoise the result of the voice search per (singer voice hash, MIDI note) in a small table (64 entries, ~512 B; or per song: <= 25 singers x the lead's ~12 distinct notes), or spread the search over two ticks. Output must be bit-identical to today's (a test compares cached and uncached blips for every template). | No slow frame on syllable onsets in the GBA slow-frame report; `test_studio.c` golden blips unchanged. |
| **MS-P2** | M | **GBA studio under budget.** From MS-P0's profile. Likely suspects from the table: the tracker redraws ~540 rects a frame at 240x160 although only the cursor, playhead and changed cells move; the SONG card's four dancing meters and spinning record. Draw the static grid / keyboard / beat shading once as a shape image (the title did this: logo, pills, hearts, 54% idle after) and redraw only dirty cells. | studio / play <= 30 ms per drawn tick on GBA (today 34.1), 30 fps; pixel-identical screenshots (same check the speed passes used). |
| **MS-P3** | S | **LYRICS text.** 213 text calls a frame vs 25-79 elsewhere: the alternating syllable colours are probably drawn per glyph. Batch per colour run (one `plat_text` per syllable), cache the syllable split per line while the line is unchanged. | text calls <= 60 on LYRICS; identical shots. |
| **MS-P4** | M | **CHOIR chibis.** 37-39 images a frame for 25 tiles: render each singer's tile once into an image-cache entry and redraw only when the singer, section or selection changes (the dialogue scene's `pc_*` re-render cost, 15% on PS1, is the same problem). | images <= 12 on CHOIR and picker; CHOIR / picker at 30 fps on GBA. |
| **MS-P5** | M | **PERFORM wall and crowd.** The show is 150-203 checker rows plus ~800 rects a frame (the concert band, at 4,600 rects, runs 19.9 fps on PS1, so the stage is not yet the problem there; it is unmeasured on GBA). Bake the LED-heart wall and the crowd silhouettes to shape images and animate them with one shade rect / palette step; keep the spotlights and the choir live. Check the PS1 display list (concert already uses 48 KB of 53 KB). | PERFORM >= 30 fps on both, checker rows <= 100 at 320x240, display list <= 40 KB. |

### C. Functionality

| ID | Size | What | Gate |
|---|---|---|---|
| **MS-F1** | S | **Redo** next to undo (the 768 B journal already stores byte ranges: keep the inverse on undo). Mapped to SELECT + A [Square]. | Tests: undo x N then redo x N restores the song byte for byte. |
| **MS-F2** | S | **Per-track mute / solo** in the NOTES and SONG tabs (a mask byte in the working song, not saved; or saved in an append-only nibble). Players skip muted channels. | Test: a muted track sends no `plat_tone`; shots. |
| **MS-F3** | S | **Metronome and default tempo** (the wishlist already specs them: PM20 metronome Off / On, PM21 default tempo 80 / 100 / 120 / 140 / 160). A click on the downbeat while START-looping a pattern. | Options store the two values; tests for the click timing against `song_length_ticks`. |
| **MS-F4** | M | **Range select and copy** in the tracker (hold SELECT + D-pad): copy / paste / transpose a block across tracks and patterns; today only whole pattern or whole track. | Journey test; undo covers it. |
| **MS-F5** | M | **Rhyme badge.** Beside the syllable-fit badge, mark lines whose last vowel group matches (cheap, no dictionary: compare the stressed-syllable tail). Feeds "write lyrics for me" so generated verses rhyme. | Unit tests on the 14 templates; word filter unaffected. |
| **MS-F6** | L | **8 patterns (A..H)** and **12 song pages.** Needs the save staging area (8 KB) and `.bss` to grow: a full book of dense songs is already ~5.3 KB, 12 pages would not fit. Do only with a `'SONG'` v2 that stores unused patterns at zero cost, and after the EWRAM overage is fixed. | Old saves load; GBA EWRAM not worse; PS1 stack reserve >= 64 KB. |

### D. Fun

| ID | Size | What | Gate |
|---|---|---|---|
| **MS-X1** | L | **PS1 sound upgrade.** Play the studio through the SPU: sampled / ADPCM instruments for the six instrument slots and the drum kits (the tone voices stay the GBA path), and turn on the SPU reverb for PERFORM (hall) and the studio (room): the work area is free. Same `DoomoSong`, same player API, a PS1-only backend behind `plat_tone`'s sibling in `ps1/src/spu.c`. Must keep the SFX preload and the 24-voice budget (SFX, babble, music engine). | Side-by-side recordings (`runner --wav` for the tone path, an emulator capture for SPU); no regression in `audio selftest` or the PS1 harness; SPU RAM map updated. |
| **MS-X2** | M | **Taste.** Performance and listening reward by who is in the audience: genre vs the citizen's favourite genre, a song's key / tempo vs their mood, friends in the choir. Replace the flat +6..+18 with a table; show the reaction faces in the crowd (the crowd is already drawn). | Unit tests on the reward table; the old range stays the clamp; free will / mood tests green. |
| **MS-X3** | M | **Song requests.** A citizen asks for a lullaby / a battle song / a love song (genre + a word); a song that matches (genre + keyword in the lyrics + a minimum fit) completes the request. Good for the story side quests and the news ticker. Reuse the quest engine; no new scene. | A quest-engine test and a script playthrough; no impact on `story-check`. |
| **MS-X4** | S | **Songs in the world.** Favourited song-book pages join the Library jukebox and can be set as the home music (the music engine plays a `DoomoSong` as a bed). First step: the jukebox only. | Jukebox lists them; the player pauses and restores music as today. |
| **MS-X5** | S | **Song of the day.** "Inspire me!" with a seed from the date: everyone's island gets the same daily seed (deterministic, so a tested golden per seed). | Golden test for three seeds. |
| **MS-X6** | M | **Karaoke / beat battle** (stretch). Play along on PERFORM: press A on the syllables (or the beat) for stars; with the co-op heroes (MP1) as a 2-player duet. Uses `song_pos_*` and `song_beat_age()` only: no player changes. | Journey test; fits the scene budget. |
| **MS-X7** | S | **Four more voices.** A kazoo (the U69 "Kazoo babble" hook) and a "citizen ooh" instrument from the babble voices, unlocked by stats like the existing ones. | New unlock ids stay in 240..255 or extend the range with a test. |

## 4. Order

1. **MS-P0** (everything else is gated by it) and **MS-P1** (a pure win, no visuals change).
2. **MS-P2 .. MS-P5** in the order the profile says; stop when GBA is under budget and PERFORM holds 30.
3. In parallel with 2, the small functionality packages: **MS-F1, F2, F3**, then F4, F5.
4. Fun: **MS-X2** and **MS-X4** first (cheap, visible), then **MS-X1** (the big PS1 win, needs the PS1 harness), then X3, X5, X7; X6 and MS-F6 last.

## 5. Risks and open questions

* **GBA memory** is the real limit: every package that adds state names its bytes and where they live. MS-F6 is blocked on the EWRAM overage.
* **Determinism:** the composer, the player step arithmetic and the voice tuning are covered by goldens; MS-P1 and MS-X1 must not change what the GBA / native path plays.
* **Device numbers are not mine:** the PS1 and GBA fps figures come from your docs (the draw-call table was measured on the native runner in a chat session). A session can build the GBA ROM and run the mGBA harness (apt: `gcc-arm-none-eabi libnewlib-arm-none-eabi libmgba-dev`, see `ROADMAP.md`), so MS-P0's GBA half can be done in a chat.
* Open: should songs reach the music engine (MS-X4 later steps) as beds, or stay in the jukebox? Should the daily song (MS-X5) be per language / per district?
