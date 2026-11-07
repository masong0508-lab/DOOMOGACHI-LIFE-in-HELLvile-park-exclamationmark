# PERFORM at 30 fps: the options being worked on (GBA)

Written 2026-11-07 (nov07k). Follows `docs/STUDIO_PERFORM_FIXES.md` (parts 1-3) and `docs/MUSIC_STUDIO_ROADMAP.md` (MS-P4).
**Option A (the ARM `vid_blit`) is built (nov07m, section 7); B is built in a smaller form (nov07n, section 8); C is not.** Every number is from the GBA harness (`gba/harness/scripts/studio_sung.txt`, 330 ticks after the first sung line).

## 1. Where we are and what 30 fps means

| | median work / tick | logic ticks/s | drawn fps |
|---|---|---|---|
| nov07g (start) | 57.7 ms | 25.5 | ~14 |
| after parts 1-2 (now) | **47.0 ms** | **29.9** | ~20 |
| after option A (nov07m) | **40.5 ms** | 30.0 | ~20 (worst 46.4, 0 ticks over 50.2) |
| goal | **< 33.3 ms** | >= 29.5 | 30 |

The GBA draws a tick in 1, 2, 3 or 4 vblanks (16.7 ms each): under 33.4 ms is 30 fps, under 50.2 ms is 20 fps. **The gap is -14 ms.**
The tempo is already fixed (logic 29.9); this plan is only about drawn frames per second.

Where the time is (switching one group off at a time from the 47.0 ms tree):

| group | cost | notes |
|---|---|---|
| 25 choir chibis | 13.7 ms | blit ~0.3 ms each + `bodyspr_image` lookup ~0.14 ms each |
| 12 crowd silhouettes | 8.0 ms | same two costs, `IMG_SILHOUETTE` |
| wall dots (unlit LEDs) | 2.8 ms | 3 rects each |
| karaoke shade | ~2 ms | cannot be made cheaper exactly |
| the rest (spots, curtains, HUD, mouths, notes, text) | ~20 ms | already trimmed in parts 1-2 |

The 37 chibi images a frame (25 + 12 + the conductor) are the only group big enough to close the gap. Everything below attacks them.

## 2. The rules that decide what is allowed

1. **The screen must not change** (the owner's rule for this work): every change is checked pixel for pixel (section 6). A change that cannot be exact is *listed here, not built*, until the owner agrees.
2. **IWRAM has little slack.** The old all-C `vid_blit` (0x4c4 bytes) filled it exactly; since nov07m `vid_blit` is 0x25c (ARM, scale 1 without flip) + `vid_blit_c` 0x240 (the rest) = 0x49c, **96 B of real slack** (`__iwram_stack_limit - __iwram_bss_end`). Resident code must not grow past it (the linker fails with `IWRAM overflow`). EWRAM has about 20 KB free (`tools/ram_ledger.sh`).
3. **VRAM reads stall the CPU** while the PPU draws: an opaque pair must be stored without reading VRAM. Three C rewrites of `vid_blit` that ignored or diluted this were *slower* (+2, +4.7, +10 ms, see `STUDIO_PERFORM_FIXES.md` part 3).
4. One patch zip per part, each measured on the ROM and reverted if it does not win.

## 3. The options, in the order they will be done

### A. Hand-written ARM core for `vid_blit` (scale 1, no flip) - exact, low risk

* **Why:** the loop is ~26 cycles per pixel pair (192 pairs a chibi, 37 chibis a frame = ~11 ms of the 47). C cannot beat the original (three tries); an ARM loop can drop the branch penalties (3 cycles each on the ARM7TDMI) and the redundant compares.
* **What:** replace only the aligned-source and odd-source inner loops (and the silhouette byte loop) with one ARM routine in `gba/src/video_loops.s` (resident, like `vid_rect_narrow`), *in the same or fewer bytes*. Rules for the loop: a source pair that is 0 is skipped; an opaque pair is stored with no VRAM read; an edge pair is the only read-modify-write; unroll by 2 with one combined test for the opaque case; the silhouette case shares the loop (colour replaces the pair's non-zero bytes).
* **Gain (estimate, unmeasured):** -3 to -5 ms on PERFORM, and a small win in every scene that draws sprites. Exact by construction.
* **Gate:** `vid_blit` + new code <= 0x4c4 bytes (`arm-none-eabi-nm -S`); a native-side exhaustive test of the pair semantics (all 65,536 pairs x destinations x silhouette on / off, the way `/tmp/pm.c` did for the mask formula); `profile-studio` and `studio_sung.txt` before / after; the 34-shot PERFORM check; `make -C gba` smoke and the other harness scripts (speed tour) for regressions.
* **Risk:** assembly in a tight space. If the first version is not faster on the ROM it is dropped.

### B. Stage image memo (skip `bodyspr_image` every frame) - exact if it is invalidated properly

* **Why:** `bodyspr_image` is called once per chibi per frame (res lookup, look-hash slot, 48-byte gene compare, `look_run_hash`, cache scan). Reusing the image pointer saved **3.4 ms for the 25 choir chibis** in an ablation (not exact on its own); the crowd's 12 + the conductor are the same again, about **-5 ms in all**.
* **What:** a small table in `portrait.c`, keyed by (resident id, dir, frame), holding the cache entry and the inputs it was built from, returned without the full lookup while all of these are true: the portrait cache epoch is unchanged (a counter bumped by `cache_alloc` / `cache_drop` and by every look-affecting write), the resident's gene / species / status are unchanged (an equal 48-byte compare is cheap inline, it was the `memcmp` *call* and the hash walk that cost), the run-hash pool is unchanged. Anything else falls back to today's path.
* **Open question to settle first:** is there a place that sees *every* write to a resident's look / status? If not, add a `res_look_epoch` bumped from the few writers (creator, ageing, status changes, family import). That audit is the real work of this option.
* **Gate:** a test that mutates each input (gene byte, species, status, evicts the entry) and checks the returned pointer / pixels equal the uncached path; shots identical; `studio_sung.txt`.
* **Gain:** -3 to -5 ms. Also helps CHOIR, the picker, concerts, dialogue (the same call everywhere).

### C. Choir chibis as hardware sprites (OBJ), hybrid - the big one, exact only under conditions

* **Why:** it is the only option that removes the chibi *blit* instead of speeding it up: the choir's 13.7 ms becomes a few ms of OAM / tile uploads.
* **What the engine already has:** squash OBJs and figure OBJs (`gba/src/obj.c`, `plat_figure_obj`), with a probe-then-checksum scheme that only shows an OBJ when nothing drawn later covers its box. OBJ tile VRAM is 16 KB (0x06014000); the squash slots take 4 x 4 KB and the overworld billboards the rest, none of which a studio frame uses (no 3D view). A 16x32 chibi (16x24 padded) at 8bpp is 1 KB: **about 16 sprites fit, 32 at 4bpp** (4bpp is a visual change: ask first).
* **What stops it being exact today, and how the hybrid handles each:**
  * *Drawn after the chibis in the bitmap, so it would sit under an OBJ:* the follow spot (over the lead only), the **mouths and notes** (13 singers), the curtain strips at the screen edges, the title / applause HUD at the top, the karaoke bar at the bottom. The hybrid keeps in the bitmap every chibi any of these touches (the lead, the edge columns, the back row if the HUD reaches it) and bakes the mouth into the OBJ image (two mouth states = two tile sets, uploaded when the beat phase flips), and the floating notes stay bitmap only if they are never over an OBJ (they are drawn over the sprite's head).
  * *Overlap between neighbours:* OBJ order follows OAM index, so painter's order (back rows first) is the OAM order; sprites overlap correctly.
  * *The crowd cannot be OBJs:* the karaoke bar **shades** the crowd (`plat_shade_rect`), an OBJ is not shaded. The crowd stays option A + B.
* **Gain (estimate):** -8 to -11 ms for the choir if ~16 can be sprites and the rest stay bitmap. With A + B (-8 to -10) that is **47 -> ~27-32 ms**: the first path that reaches the goal.
* **Gate:** the 34-shot native check proves nothing for OBJs (the native runner draws everything in the bitmap): use the **GBA harness shots** with `sync on` at fixed ticks (`MUSIC_STUDIO_ROADMAP.md`, "How to check"), old ROM vs new, `cmp` of the PNGs, plus the probe / cover checks of `obj.c`. This needs a new, deterministic GBA comparison script first (part C0).
* **Risk:** highest. Mode 4 OBJ priority, OAM budget, tile upload cost per frame, interaction with `postfx` shake / zoom. If the cover rules leave fewer than ~8 sprites, the gain is too small and it is dropped.

## 4. Order, parts and the expected total

| part | what | cost to reach | expected result |
|---|---|---|---|
| A | ARM `vid_blit` core | -3..-5 ms | 47 -> ~42-44 (**landed nov07m: 47.0 -> 40.5**) |
| B | stage image memo | -3..-5 ms | -> ~38-41 |
| C0 | GBA harness exact-compare script (deterministic ticks, `cmp`) | 0 | needed before C |
| C | choir OBJ hybrid | -8..-11 ms | -> ~28-33 (**goal reached only if all three land**) |

If A or B does not measure as predicted, C is still the lever; if C fails its cover analysis, 30 fps is not reachable *exactly* and the remaining choices are the non-exact ones in section 5.

## 5. Not being worked on (needs the owner)

* **Half-rate crowd / wall** (draw them every other frame): saves up to ~8 + 3 ms, but it is a visible change (stutter in the crowd).
* **Fewer / simpler silhouettes** (8.0 ms for 12): visible.
* **4bpp chibis for OBJs:** doubles the sprite count, but changes the look.
* **F1, karaoke bar as a retained panel:** not possible exactly (the shade is not retained, the text lies on the shade).
* **PS1:** unmeasured; the 3-vblank cliff is a GBA figure. None of A-C touches the PS1 path.

## 6. How every part is checked (the same each time)

1. **Native exactness** (A, B): `studio_perform` shots at 240x160 and 320x240 on the old and the new tree, `cmp` (34 shots; the script is the navigation of `core/tests/scripts/studio.txt` up to the stage, then `press A` and `shot` every 9 ticks). A deliberately wrong variant must break it (the check has to be sensitive).
2. **GBA exactness** (C): harness shots at fixed ticks with `sync on`, `cmp`.
3. **Cost:** `build/harness build/doomogachi.gba --script gba/harness/scripts/studio_sung.txt --no-fps-gates --worklog --quiet > sung.log` then `python3 tools/worklog_median.py old.log new.log`. Gates for the whole job: **sung steady median < 33.3 ms, logic >= 29.5 ticks/s, intro spikes 0, worst tick < 50.2 ms.**
4. `make -C core test` green; `make -C gba` builds (no `IWRAM overflow`); `tools/ram_ledger.sh` for EWRAM; the other harness scripts (`speed_tour.txt`) show no regression.
5. Report honestly in the commit message: the figures measured, and what was not measured (PS1, the intro window).

## 7. Option A: landed (nov07m)

nov07l tried a first ARM `vid_blit` (four body copies, 944 B + the C fallback 576 B = 1,520 B, 300 B over the IWRAM gate) and stopped. nov07m is the second try, in the shape the nov07l notes planned.

**What it is.** `vid_blit` is now ARM in `gba/src/video_loops.s` for scale 1 without flip (cached chibis, building sprites, shape images, silhouettes); anything scaled or flipped is one `b vid_blit_c` away (`video.iwram.c`, the old C with its scale-1 branch removed: 576 B). 604 B for the ARM routine, 1,180 B in all, **40 B under the old 1,220 B**. How it stays small: one shared tail (`BLIT_TAIL`) for all three bodies (aligned source, odd source, silhouette), each body only produces the pair `v`:

* a pair is `v` = two pixels in one halfword; `v == 0` is skipped; both bytes set is one `strh` with post-increment (no VRAM read); one byte set is the only read-modify-write; the end of the row is tested by comparing `p` with the end pointer, so an opaque pair costs about 12 instructions and one branch;
* an odd source row reads aligned halfwords and builds each pair from the high byte of one and the low byte of the next (the C code's trick); a silhouette reads bytes and turns each set byte into the colour, then uses the same tail;
* the odd first pixel and odd last pixel of a clipped row are done outside the loop, the same way the C code did.

**Measured (GBA harness, libmgba, `studio_sung.txt`, 330 ticks after the first sung line):** median **47.0 -> 40.5 ms** (steady 47.2 -> 40.7), worst 52.9 -> 46.4, ticks over 50.2 ms 2 -> 0, logic 29.9 -> 30.0 ticks/s, drawn ~16.6 -> 20.1 fps. That is -6.5 ms, better than the -3..-5 estimate, because silhouettes (12 a frame) gained as much as chibis. Studio profile (`studio_profile.txt`): CHOIR 46.5 -> 42.3 ms, PERFORM intro windows 42.5 / 42.3 -> 36.0 / 35.9 ms. Speed tour: concert +7.9% fps (13.7 vs 12.7), combat +2..3%, island town hall menu +4.7%, every other scene within +-0.7%.

**Exactness (three independent checks):**
1. `gba/enginetest/et_blittest.c` (new, runs at engine-test boot, `ET blittest ok (6000 cases)`, expect-log in `enginetest.txt`): the new `vid_blit` against a verbatim copy of the old all-C one, 6,000 random images (widths 1-40, odd and even, 4 source alignments, 0-75% transparent), positions clipping on all four sides, scales 1-3, flip, silhouette, over a random background so the edge pairs' read-modify-write counts; both VRAM pages compared row by row after every call and in full at the end. A deliberately wrong variant (the trailing pixel's mask 0xFF00 -> 0xFE00) fails it at case 16: the check is sensitive.
2. Speed tour with `--parity` against the old ROM: all 16 scenes, 30 frames each, identical output frames.
3. `vhash-seq 150` over the sung PERFORM phase (ticks 1721-1870): sequence digest `40422a40` on both ROMs. (Plain harness `shot`s are *not* comparable here: the faster ROM lands the shot on another animation tick; see `MUSIC_STUDIO_ROADMAP.md`.)

`make -C core test`: 873 passed, 0 failed. Not measured: PS1 (untouched by this change), a real GBA (libmgba timings only; VRAM stall behaviour on hardware may differ a little).

**What is left to 33.3 ms:** -7.2 ms. Next in order: **B**, the stage image memo (-3..-5 ms), then C0 / C. Cheap extras for the blit itself, not done: unroll the opaque loop by two (saves about 4 cycles a pair), and test `v == 0` on a 32-bit load to skip two transparent pairs at once; each costs IWRAM bytes (96 B free).

## 8. Option B: landed in a smaller form (nov07n)

**What it is.** Not the whole stage image memo: the per-body hint inside `bodyspr_image` (`core/src/portrait.c`). (1) The look-hash memo compares the 48 gene bytes word by word when both records are 4-byte aligned (`gene_same`; `memcmp` stays as the fallback for unaligned records, which trap on the R3000). (2) Each memo slot (`BodyLH.ei`) remembers the cache entry its body was last found in; that entry is tried first with the same test `cache_get` applies (kind, not busy, resident, look hash incl. the deep run, key) and touched the same way (stamp, tick), so eviction order is unchanged. Any miss takes the old path.

**Measured** (studio_sung.txt, 330 ticks, GBA harness): median work 40.5 -> 38.5 ms, **mean saving 1.95 ms** on the 172 ticks both builds drew (the estimate for B was -3..-5 ms). IWRAM unchanged (25,760 B used). `vhash-seq` over the same 330 ticks is identical to the baseline (`242b1074`): exact. The word compare alone gave 0.4 ms; the hint is most of the gain.

**Check.** `bodyspr_hint_follows_every_input` (core/tests/test_portrait.c): warm answers equal a cold-cache render across facings and frames in three orders, look genes, species, status, deep runs put and dropped, invalidation and 80 looks that reuse every block. Mutations that drop the key, look or word-0 compare, or make `gene_same` always true, fail it. Not caught, and harmless for pixels: dropping the resident or busy check (identical looks draw identical pixels; busy only occurs in the GBA's sliced jobs) and ignoring gene bytes 44..47 (none shows on a chibi).

**Open.** One tick near the end of the sung phase (1639) costs 51.7 ms against 46.4 ms before; it also appears with the silhouette draw removed, so it looks like a tick that now falls on a different phase of the song rather than a cost of this change; not explained yet. What is left to 33.3 ms is about -5 ms: next C0 / C.

## 9. A quieter crowd (nov07o) - a visual change, by request

Not exact: the frames differ from nov07n on purpose. `STUDIO_CROWD` 12 -> **11** (a row of 6 in front of a row of 5), and the crowd's animations are reduced: nobody bobs on the beat any more, glow sticks show only on every other fan and only above 70 % applause (they used to flicker on for one fan in five all the time), and at most two hearts float up (was five). The singers, conductor, glows, spots and curtains are untouched.

**Measured** (studio_sung.txt, GBA harness): median work 38.5 -> 37.6 ms (steady 38.8 -> 37.9), 2.7 ms below the nov07m baseline; worst tick 50.8 ms (one tick over the 50.2 ms line, the same song tick as in section 8). IWRAM unchanged. `make -C core test`: 874 passed. Not measured: PS1, the intro window, the native 34-shot check (its shots change by design). Left to 33.3 ms: about -4.3 ms, which is Option C's job (section 3).

