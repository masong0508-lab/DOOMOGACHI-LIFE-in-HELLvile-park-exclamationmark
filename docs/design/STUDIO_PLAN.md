# STUDIO plan: a friendly, playable song studio with an Advanced mode

Written 2026-11-15 by the STUDIO lead (ST-0). Owner brief: "vastly improve the song studio to be more user
friendly and intuitive at the same time, with particularly detailed tool functionality and playability, and the
possibility to have advanced options to make full-fledged songs" (`held/OWNER_DECISIONS.md`, "SONG STUDIO,
FRIENDLY AND FULL"). The studio as built: `docs/STUDIO.md`. Standing brief: `held/leads/STUDIO.md`.
Defaults in this file are chosen by Claude (STUDIO); the owner may override them.

## 1. What is there now (measured and played, 2026-11-15)

Seven tabs: SONG, NOTES, ORDER, LYRICS, CHOIR, PERFORM (STAGE), FILE. Native shots of every tab at 240x160 and
320x240 come from `core/tests/scripts/studio.txt`. Format: `DoomoSong`, 892 bytes, 4 tracks x 4 patterns x 32
steps, an order list of 16, 8 lyric lines, 16/24/25 singers. Output: the four `plat_tone` channels. Book: 6 pages
in the `'SONG'` save chunk. Core tests pass.

### Speed (GBA, libmgba, `make -C gba profile-studio`, last full table 2026-11-07, `docs/STUDIO.md`)
| window | work ms (budget 33.3) | fps |
|---|---:|---:|
| SONG idle / playing | 33.1 / 36 | 24-29 |
| NOTES idle / playing | 29.6 / 34.7 | 25-30 |
| ORDER | 27.1 | 30 |
| LYRICS | 36.5 | ~25 |
| CHOIR | 42.3 | 20 |
| PERFORM (intro / sung) | 36.0 / 40.5 | 18-27 |
PS1: the editor tabs hold 30 (`docs/PS1.md` speed pass 3); CHOIR and PERFORM are not measured there. Dreamcast: no
studio measurement exists; the 2D tests only. SPEED owns the STAGE tab until its S3-8 hands it over; the editor
tabs (SONG, NOTES, LYRICS) are ours and sit on the line, so **every new control must be paid for** with a cheaper
frame (ST-EASY re-measures first, and gives back at least what it adds).

### What is confusing (one line each)
1. A new player lands on SONG with eleven rows (swing, repeat, "Lead vol") and no hint what to do first.
2. The studio opens on an empty or half-made song: nothing says "make a song in 10 seconds".
3. Tabs are icons except the active one: nobody knows what the ear, the star or the heart icon opens.
4. NOTES is a 4 x 32 tracker with hold-A-plus-D-pad editing: not discoverable, and no way to hear a note without placing it.
5. Wrong notes are possible (chromatic scale, out of key notes in a pattern) and nothing marks them.
6. ORDER, patterns "A..D" and "order passes" are two ideas for one: "which part plays when".
7. Lyric lines are tied to order positions by index ("line i over position i"): invisible, and the fit badges are cryptic.
8. FILE hides the best things (templates, Inspire me!) below the book list; "New blank song" erases without a guide.
9. SELECT is overloaded: tools menu, SELECT+L/R tab, SELECT+B undo. B means five things.
10. No redo, no mute / solo, no metronome, no copy of a range, no loop while editing a section.
11. START plays from the top or the cursor depending on the tab; nothing says which.

### What is missing for full songs
Only 4 patterns and 16 passes (about 1 minute), 4 tracks, no chord track, no velocity / pan / envelope, no key or
tempo change, no automation, no sections, 8 lyric lines (positions 8..15 repeat them), no way to play a note live.

## 2. The new flow

**Easy is the default; Advanced is a switch on the FILE tab ("Studio level: Easy / Advanced") and on every tool
menu. Switching never loses data: Easy shows a simpler view of the same song.**

### 2.1 First screen: "Start a song" (ST-EASY)
A guided card shown when the studio opens on an empty song or on "New song":
1. **Style** (the 16 composer styles by their friendly names: Bubblegum, Chiptune, Lullaby, Punk, Doom, Goth,
   Lo-fi, Folk, Hymn, Shanty, Synthwave, Jazz, Disco, Industrial...).
2. **Mood** (Happy / Sweet / Sad / Spooky / Fierce: picks key mode and the chord progression family).
3. **Tempo** (Slow / Medium / Fast / Frantic: 3 presets of bpm per style).
4. **Length** (Short 16 bars / Song 32 / Long 64; Easy stops at Long, Advanced goes on).
Then "Make it!" runs `studio_compose` (deterministic from a seed) and drops the player on the playing song with
the **Tweak** screen. A "Shuffle" button re-rolls the seed. "Inspire me!" and the templates sit on the same card.
Old shortcuts stay.

### 2.2 Screens
Easy keeps the seven tabs but renames and reorders them to the order of a song's life; each tab has a
**one-line instruction** under the strip (`tab_help[]`, shown until the first edit of the session, then on a
"?" key) and a **control card** (a popup listing the buttons for this tab, opened by the "?" key).

| Easy tab | Was | What it is |
|---|---|---|
| START | (new) | the guided start, then **Tweak**: loudness, tempo, "more / less" sliders (busy, bright, bouncy) that recompose with the same seed + a knob |
| PLAY | (new, ST-PLAY) | jam mode and drum pads: the buttons play the song's scale and chords |
| EDIT (notes) | NOTES | the tracker, with a note grid view (ST-TOOLS) |
| ARRANGE | ORDER | parts and their order, in plain words ("Verse, Chorus, Verse, Bridge, Chorus") |
| WORDS | LYRICS | lines under the parts they are sung over |
| CHOIR | CHOIR | unchanged, plus "everyone / half / solo" quick buttons |
| SHOW | PERFORM | unchanged for now (SPEED owns the draw cost) |
| FILE | FILE | book, new, templates, undo / redo, **Studio level** |
The old SONG tab's settings (title, genre, key, scale, tempo, swing, repeat, instruments, volume) split:
the Easy basics (title, style, tempo, key by name "C major") go to START / Tweak; the rest moves under EDIT's tool
menu ("Sound"). Nothing is deleted.

### 2.3 Rules that make it friendly (all levels)
* **No wrong notes by default.** Placing, jamming and recording snap to the song's scale (`song_scale_snap`);
  a "Free notes" switch (Advanced) allows chromatic. Out-of-key notes already in a song are drawn with a red
  dot.
* **Play from anywhere**: START always plays; its target is written in the prompt bar ("Play from here" /
  "Play song"); a second press stops. SELECT+START loops the part under the cursor.
* **The cursor's item is described** on one line at the bottom of every tab ("Bar 3, beat 2, Lead: E4 (a
  third above the chord)"), localised through the existing text tables.
* **Undo and redo** (SELECT+B undo, SELECT+A redo; `L2/R2` or Triangle / Square on the PS1 and Dreamcast), with the
  step counter on FILE. The existing 768-byte run-length journal gains a redo cursor (journal slots are
  re-playable: a redo is the saved "after" image; cost: +384 B in the scratch arena, not `.bss`).
* **B always means "back / cancel"**; erase is Y / X on pads that have it, and "SELECT, Erase" on the GBA.
* Every new screen passes the comprehensibility rule: first-time text names the buttons by the pad's own words.

### 2.4 Playability (ST-PLAY)
* **Jam mode** (the PLAY tab): the face buttons A / B / L / R (and d-pad rows) play the 8 notes of the song's scale
  in the current octave, chords from the song's progression on the d-pad (I, IV, V, vi) so nothing is wrong. Notes
  sound on channel 0 with the lead instrument through `song_preview` (a held-note variant `song_jam_on/off`
  is added: that is the only player change; it needs no engine change).
* **Live recording**: REC plays the song looped with a count-in click; the notes pressed are written into the lead
  (or the chosen) track on the step grid with **quantize** (off / 1/4 / 1/8 / 1/16, default 1/16; the step is
  decided from the player's tick position, `song_pos_ticks`). A take is one undo step. Overdub keeps the old notes
  (Advanced: replace).
* **Loop while editing**: `song_play_pattern` already loops a pattern; extend it to loop a **range of steps** (a bar
  or a selection) so you hear what you change.
* **Drum pads**: the same keys play the kit (kick, snare, hat, open hat, clap, tom, crash, cowbell) and record into
  the drum track with the same quantize.
* The GBA has 10 keys: A, B, L, R, 4 d-pad; SELECT is the shift (octave / chord set), START is play. The PS1 and the
  Dreamcast pads add 4 more face buttons, so jam there has two rows. The keyboard of the PC runner keeps its
  existing mapping.

### 2.5 Tools (ST-TOOLS)
* **Note grid**: a piano-roll view of one track and one pattern (rows = scale degrees of two octaves, columns = steps),
  d-pad to move, A to place or extend (hold A + RIGHT draws a long note), cursor row sounds when it moves.
* **Selection**: hold SELECT + d-pad draws a rectangle; then copy / cut / paste / delete, **transpose** by scale
  step or octave, **repeat** (fill the range to the end of the pattern), **reverse**, **double / halve**. One
  clipboard of 1 pattern x 1 track (96 bytes in the scratch arena).
* **Per track**: instrument, **volume and pan** (pan is a GBA PSG left / right flag and a PS1 / DC voice volume
  pair; on the old 4-channel player it maps to "left / middle / right"), a simple **envelope** (soft / punchy / long
  / pluck: four presets that map to `plat_tone` volume steps, and to ADSR on `plat_voice`), mute and solo,
  **swing per track** (the song's swing stays the default).
* **Chord track**: a fifth lane that holds one chord per step range (root + quality). The harmony and bass tracks may
  follow it ("Follow chords": arpeggio, pad, stabs, roots, walking: the composer's styles), and the jam chords come
  from it.
* **Drum pattern editor**: eight lanes by 16 steps with one button per cell, plus the 15 composer grooves and fills as a
  starting point and per-lane mute.

### 2.5b Helper tools (owner, 2026-11-15: all seven, on every platform; a lighter version where tight, never dropped)
All are menu items on the tab they help, deterministic (seed + inputs), one undo step each, and suggest rather than force.
| Tool | Where | What it does | Lighter on a tight platform |
|---|---|---|---|
| Chord suggester | EDIT (chord lane) / ARRANGE | offers 4 next chords for the section from the key, mood and the previous chord (diatonic table, no search) | 3 suggestions, no preview loop |
| Melody helper | EDIT (lead) | "Suggest a phrase": 4 candidate one-bar motifs on the current chords, from the composer's motif code | 2 candidates |
| Groove picker | EDIT (drums) | the 15 composer grooves + fills by name, auditioned on the current bar | names only, no audition loop |
| Lyric syllable fitter | WORDS | today's auto-fit plus "show the stress": which notes get which syllable, fix by one press | fit badge only |
| Auto-harmony for the choir | CHOIR | writes harmony lines for sections (thirds / fifths / octaves per section) from the lead, into the harmony track or `choir_double` | thirds only |
| Section copier | ARRANGE | copy a section (with its words and singers) to a new slot, optionally transposed or with the drums swapped | copy only |
| Mix helper | EDIT (Sound) | balances track volumes by role (lead over harmony over bass), flags tracks that hide each other, one-press "balance" | balance only |

### 2.5c The detail gate (owner, same day)
"Detail to functionability": every function does exactly what its name says, gives feedback (a sound, a toast or a changed
card), can be undone, and has no dead end or half-working option. A package does not land until its **function ledger**
(`docs/STUDIO.md` section "Function ledger", one row per control: what it says, what it does, feedback, undo, tested by)
has no empty cell. Options that cannot work on a platform are hidden there, never greyed with no reason.

> **Built so far (2026-11-15, core only; docs/STUDIO.md "ST-ADV"):** the format, the player and the save are done. Where the code
> differs from this plan: the pages are a tail of the `'SONG'` chunk (version 1), not a new `'SNG2'` chunk, so no save slot or budget
> line was needed; patterns stay 2 bars; lyrics are the classic 8 lines, a section names the line of each pass; a `SongExt` is 2,176 B
> (the plan said 3.5 KB); the GBA and PS1 keep 0 Advanced pages until a resident share is granted.

### 2.6 Advanced (ST-ADV)
Advanced is a level switch that unlocks: more of everything, automation and expert views. Off by default; the
old four-tab song opens in it unchanged.
* **Sections**: a song is a list of *sections* (intro, verse, pre-chorus, chorus, bridge, outro; named, coloured,
  1..N patterns each) and an arrangement list of sections (up to 64 entries, 8 bars each, so a song up to 8 minutes
  at 120 bpm). Easy's "parts" are the same sections with the names hidden behind the six defaults.
* **Patterns**: up to 32 patterns of 1..4 bars (the classic 32 steps = 2 bars stays the unit); the song keeps a
  `pattern_count`.
* **Key and tempo changes**: per section transposition (-12..+12) and tempo (%, or a bpm), plus a "key change up one
  step for the last chorus" quick tool.
* **Automation**: per-section volume ramps (fade in / out) and a filter-ish "brightness" (the instrument duty cycle
  on pulse waves; ADSR on SPU) on the lead.
* **Tracks**: 4 on the old player (as now); on engines that give more voices the song may use up to **8** (6 melodic
  + 2 drum / FX): PS1 and Dreamcast through `plat_voice`, the GBA by the 2..11 voices the scene tier of the music
  engine hands out (`docs/AUDIO.md`; the studio is a "tier 2" scene). The count a platform plays is
  `song_tracks_max()` (a compile-time constant per platform); a song with more tracks than that **folds** the extra
  tracks onto the nearest lane (and says so on FILE: "This song has 8 tracks, this console plays 4: tracks 5..8 are mixed in")
  so a song written on a Dreamcast still plays on a GBA.
* **Lyrics across the whole song**: lyrics are addressed by section and line (a line = one lead phrase), not by order
  index; up to 4 lines per section; the choir assignment (`line_singers`) is per line.
* **Expert views**: the per-note velocity bar, the free-note switch, a song-wide note count and a memory meter.

## 3. Song format changes (versioned; old songs load)

The classic `DoomoSong` (892 bytes, 4 x 4 x 32) is **not touched**: every field, the book and the `'SONG'` chunk
stay valid and the existing tests (`test_studio`, the `music_golden` composer data) hold.

New, appended to the book page and saved in a **new chunk `'SNG2'`** (the next free save tag is checked at ST-ADV
time with `core/include/doomo/save_ext.h`):
```
SongExt {            /* per book page, variable length, null if the song is classic */
  u8 version, tracks (4..8), sections (1..16), arrange_len (1..64), patterns (4..32);
  u8 flags;          /* bit0 Advanced song, bit1 free notes */
  Section  sect[16]; /* name id (0..5 default or custom 8 chars), pattern list (<= 8 ids), key shift, tempo pct */
  u8 arrange[64];    /* section ids */
  PatBlock pat[32];  /* the patterns > 4 and the extra tracks, RLE as the classic chunk does */
  TrackMeta trk[8];  /* instrument, vol, pan, env, mute / solo not saved, swing */
  Chord chords[..];  /* chord track as (step, root, quality) triples */
  u8 lyric_ext[..];  /* lines per section, who sings them */
  Auto auto[..];     /* (section, param, value) triples */
}
```
`song_sanitize()` clamps it. A classic song is `version 0` and reads as one section per order slot. Loading an
Easy song never allocates. An Advanced song that has not been edited in Advanced saves as classic.

The player gets `song_play_ext(const DoomoSong *, const SongExt *)`; the classic `song_play` calls it with `NULL`.
Rules from `OWNER_DECISIONS.md` hold: PG-21 words only; the word filter and the choir rules (minors never in
suggestive songs, `mature_adult_ok` not weakened) apply to every lyric and every title.

## 4. Memory per platform (the gates)

| | GBA | PS1 | Dreamcast |
|---|---|---|---|
| resident `.bss` added | **0** (net zero; EWRAM about 2 KB free, IWRAM nearly full) | **0** until the coordinator sets a share; ask with numbers | no limit seen (16 MB); 4 KB |
| scene scratch (`scene_scratch()`, only while the studio is open) | classic `Studio` today ~2 KB; ST-EASY adds redo (384 B) and the clipboard; ST-ADV the working `SongExt` (<= 3.5 KB) and an undo for it. Target <= 8 KB total; measured with `make -C gba sizes` | the same | the same |
| the song book | 6 pages as now (5.4 KB). Advanced pages live in the save: each page <= 3 KB, 2 Advanced pages max on the GBA (save staging is 8 KB; `docs/STUDIO.md` "Persistence") | 4 KB / 3 pages | 6 |
| code | GBA code is in ROM (32 MB): only the EWRAM / IWRAM functions are counted | the overlay slot has 126,976 B and the studio group is 45,524 B: ST-EASY + ST-TOOLS add about 18 KB, ST-ADV about 12 KB: still inside | none |
| FPS rule | 30 fps in every studio window; GBA work <= 30 ms | 30 fps (EXE margin: the 72 KB gate is untouched: no resident share) | 30 fps |

How the 0-resident rule is met: all new editor state (selection, clipboard, redo, grid cursor, the working
`SongExt`) is `scene_scratch()`; the 6-page book stays the only resident data. Advanced songs on the GBA do not
need new `.bss` because the book pages that hold them reuse the 892-byte slot for the classic part and the
variable `SongExt` is read from the save buffer into scratch on open. A 2nd platform tier can enlarge the budget
later, never this plan.

## 5. Look (ST-USE)
One style (the owner's decluttering decision of 2026-11-15): a calm card layout, the tab strip shows labels (the
active tab spells its name, the rest are numbered and the L / R hint shows the neighbours' names), one accent colour
per track (lead pink, harmony mint, bass lilac, drums amber, chords sky) and every popup uses the same card. The
studio's backdrop cost (about 14 ms per tab) is cut by the retained panel, not by removing the look.

## 6. Package order and gates

| Package | What | Gate |
|---|---|---|
| ST-0 | this plan; machine setup; the current numbers | landed with this file |
| ST-EASY | START card, Tweak, `tab_help`, control cards, undo / redo, play-from-anywhere, cursor description, renamed tabs | tests: composer determinism, undo / redo round trip; speed2 before / after |
| ST-PLAY | `song_jam_*`, REC with quantize, range loop, drum pads | tests: quantize unit tests, jam stays in scale, 1 undo per take |
| ST-TOOLS | note grid, selection, transpose / repeat, per-track mix, chord track, drum editor, the seven helper tools (2.5b) | tests per operation, old songs load |
| ST-ADV | `SongExt`, sections, key / tempo change, automation, 8 tracks, lyrics across the song, `'SNG2'` | save round trip, old-save test, folding test, GBA sizes |
| ST-USE | user songs in the Concert Hall, PERFORM and the show; the declutter pass | the show tests; shots before / after |

Each package: speed2 before and after, memory gates, tests for every new rule, shots at 240x160 and 320x240 (checked
by looking), docs (`docs/STUDIO.md`, `ROADMAP.md`), and one push.

## 7. Open points
* The audio engine's track count per platform (`song_tracks_max()`) needs a look at `mus_*.c` and the platform voice
  layer; ST-ADV does that first and writes the real numbers here. Any engine change is `cross-stream:` and the
  smallest possible.
* The PS1 resident share and the save staging growth are asked of the coordinator at ST-ADV, with measured numbers.
