# The Music Studio

*"Make a way to edit lyrics and music in the game."*

The Music Studio (`SCENE_STUDIO`) is where you write songs for the citizens of
Hellvile Park to sing: the tune on a 4-track tracker, the words line by line,
and a choir of up to **25 citizens** (host build; **16 on the GBA, 24 on the PS1**, set by `SONG_SINGERS` in `song.h`) who sing it on stage. Every song uses one
shared format, `DoomoSong` (`core/include/doomo/song.h`), and one shared
player, so the Concert Hall's generated songs and your own are the same thing.

Planned work (fps, functionality, fun): [MUSIC_STUDIO_ROADMAP.md](MUSIC_STUDIO_ROADMAP.md).

| File | What |
|---|---|
| `core/include/doomo/song.h` | the contract: `DoomoSong`, the player API, the song book |
| `core/src/song_player.c` | the player + helpers (timing, singing, choir, syllables, scales) |
| `core/src/song_book.c` | `doomo_song_book[6]` and the `'SONG'` save extension chunk |
| `core/src/studio.c` | the scene: tabs, input, undo / redo journal, popups, citizen picker, unlock rules, `st_easy()` |
| `core/src/studio_edit.c` | SONG, NOTES (tracker) and ORDER tabs |
| `core/src/studio_play.c` | PLAY: jam pads, live recording with quantize, range loop, drum pads (ST-PLAY) |
| `core/src/studio_words.c` | LYRICS and CHOIR tabs |
| `core/src/studio_stage.c` | PERFORM: the choir show |
| `core/src/studio_file.c` | FILE: song book, new song, templates, Inspire me, undo / redo, Studio level (scrolls) |
| `core/src/studio_start.c` | ST-EASY: the "Start a song" card, `studio_make_song` |
| `core/src/studio_easy.c` | ST-EASY: START plays and names its target, the NOTES cursor line |
| `core/src/studio_help.c` | ST-EASY: one-line help per tab, the control card, tab labels, the Studio level |
| `core/src/studio_gen.c` | composer, lyric writer, 14 templates, choir fills, parts, auto-fit, word filter |
| `core/src/unlocks_studio.c` | unlock ids 240..255 and the activities |
| `core/tests/test_studio.c`, `core/tests/scripts/studio.txt`, `studio_easy.txt` | tests and the runner tours |
| `core/tests/test_studio_play.c`, `core/tests/scripts/studio_play.txt` | the PLAY tab: tests and the runner tour |

You find it in the **Concert Hall** (activities table `doomo_activities_studio`,
free play and story from chapter 1). (CL5, clarity C9: it is no longer a second
row in the Town Hall, which keeps its civic rows only.)

## Controls

GBA buttons only; PS1 extras in brackets. Each tab's prompt bar (C3,
`tab_prompts` in `studio_edit.c`, the shared `ui_prompts`) names the pad's
own buttons ("SEL+B Undo" on a GBA, "Triangle Undo" on a PS1) and gives way
item by item at 240x160. On the SONG tab its left / right verb says what the
row is ("Swing feel", "Loudness", "Instrument"...), the beating heart top
right reads "110 bpm" where it fits, and the NOTES header says where the
pattern plays as "in Order 2,3,5" (its slots in the ORDER tab).

| Anywhere | |
|---|---|
| L / R | previous / next tab (in NOTES: previous / next pattern; in PLAY they are pads: use SELECT + L / R) |
| SELECT + L / R | previous / next tab from any tab [L2 / R2] |
| SELECT (tap) | the tab's tools menu |
| SELECT + B | undo [Triangle] |
| SELECT + A | redo [Square] (ST-EASY) |
| SELECT + UP | "?": the control card of the tab (ST-EASY) |
| START | play / stop, the prompt bar names the target: "Play song", "Play from here", "Stop" (ST-EASY) |
| B | leave the studio (asks if the song is unsaved; in NOTES: erase / tie) |

### Easy mode (ST-EASY, 2026-11-15)
The first package of the studio overhaul (`design/STUDIO_PLAN.md` 2.1, 2.2, 2.3). Easy is the only level there is
for now; the Studio level switch (FILE) is stored and shown, Advanced adds nothing yet (ST-ADV).

* **Start a song.** The studio opens on it when the song is empty (a new save, or an empty page) and FILE's
  "New song..." opens it again. It lives on the SONG tab (`studio.card`; the picks, the seed and the row are in
  `st_easy()`). Rows: **Style** (16 short names: Bubblegum, Lo-fi, City Pop, Chiptune, Lullaby, Jazz, Classical, Folk,
  Disco, Shanty, Hymn, Synthwave, Goth, Punk, Doom, Industrial), **Mood** (Happy = major in C, Sweet = pentatonic in
  F, Sad = minor in A, Spooky = harmonic minor in E, Fierce = blues in D: key mode, key and with them the chord
  family), **Tempo** (Slow, Medium, Fast, Frantic: the style's own bpm range at `lo - 6`, its middle, `hi`, `hi + 20`,
  clamped 60..220; the card shows the bpm), **Length** (Short 8 passes = 16 bars, Song 12 = 24 bars, Long 16 = 32
  bars: the plan's 32 / 64 bars wait for ST-ADV, `SONG_ORDER_MAX` is 16), **Shuffle** (a new seed, the same picks, the
  song plays and the card stays), **Make it!** (compose with the seed, play, the card closes: what Shuffle played is
  what is kept), **Inspire me!** (the FILE citizen picker), **Templates...** (FILE's template list). B skips the card
  and leaves the blank song. `studio_make_song(song, picks)` is a pure function of the picks, the seed and the
  song's choir / words (tested: all 16 x 5 styles / moods twice, equal bytes); the mood reaches the composer through
  `studio_compose_scale()` (`studio_compose` is unchanged for every other caller).
* **One line per tab** (`st_tab_help`, at most 36 glyphs so it fits the 240 px bar). It replaces the prompt bar when a
  tab opens until the first button press on it (30 s at most), and never again once an edit was made in the visit.
* **The control card** (SELECT + UP, "?" on the pads that have it): a popup with the one-line help, this tab's buttons
  and the buttons every tab has, written with the pad's own names (the prompt bar's glyph builder). A, B or START
  closes it.
* **START always plays**, second press stops, and the bar says what it plays: "Play from here" on NOTES (the pattern
  from the cursor step, looping), ORDER (from the selected pass) and LYRICS (from the pass that sings the selected
  line), "Play song" everywhere else, "Stop" while anything plays. Two old START jobs moved: LYRICS "Fit this line" is
  the last item of its SELECT tools (it was START), and CHOIR's section cycle is only in the singer's A menu
  ("Change section", it was also START).
* **Redo.** An undo keeps the bytes it overwrites in a second journal in the scene scratch (the same record format,
  `STUDIO_REDO_BYTES` 384); a redo replays it and puts the step back on the undo journal. Any new edit empties it.
  A step too big for the redo journal says "Too big to redo" instead of redoing half. The Start card's Make it! and
  Shuffle are the exception: they remember their picks and seed (a 14 byte recipe record in the same journal) and a redo
  recomposes the very same song. Shuffle and Make it! also take the previous take back first, so the card adds one small
  undo step whatever the number of takes, and one undo returns to the song before the card. FILE shows `Undo (n)` and `Redo (n)`; SELECT + A is redo on a GBA (X / Square on the others).
* **The cursor's cell on NOTES**, one line under the grid: "Bar 2, beat 3, Lead: E4 (3rd of C)", "... Harmony: F#4
  (off the scale)", "... Bass: hold", "... Drums: Snare". It falls back to shorter forms until it fits; the text is
  cached and rebuilt only when the cell, the key or the scale change.
* **Tab labels.** Start, Edit, Arrange, Words, Choir, Show, File (the old Song, Notes, Order, Lyrics, Choir, Stage,
  File). The current tab spells its name; when the strip is too narrow for all names (240x160) the others show
  L and R beside it (the neighbours that L / R reach) and their number.
* **Studio level** (FILE row, A or LEFT / RIGHT toggles): Easy or Advanced, in `st_easy()->level` and bit 15 of
  `studio_stats.lines` (`STUDIO_LINES_LEVEL`; the lyric-line count is the low 15 bits, `STUDIO_LINES()`), so it
  rides in the existing `'SONG'` chunk with no new save tag; an old save reads Easy. Advanced shows no extra tools
  yet.

### SONG
Title (typed with the on-screen keyboard), genre, tempo (60..220 bpm, hold for
steps of 5), key (moves all notes with it), scale, swing (0..8), repeat
(once / loop), and per track the instrument (Tiny Pulse, Pulse, Square, Flute,
Buzzy Saw, Static) and volume; the drum track picks a **drum kit** (Toy Box,
Bone Rattle, Boom 808, Plush Pillow, Hell Forge, Classic). The song card spins
a record, shows key, length, singers and four dancing instrument meters.
Tools: re-compose the music (keeps the words), key up / down, snap every note
to the scale.

### NOTES - the tracker
Four tracks (Lead, Harmony, Bass, Drums) x 32 steps (two bars of 16ths) per
pattern A..D, scrolling, with beat shading, a bar line, the playhead and a
bouncing heart cursor. A mini keyboard shows the current note and marks the
notes of the key/scale with mint dots.

| | |
|---|---|
| D-pad | move |
| A | place the last note (snapped to the scale), or pick up the note under the cursor; the cursor then steps down |
| hold A + UP / DOWN | one scale step (chromatic scale: a semitone); drums: the next / previous drum |
| hold A + L / R | an octave down / up |
| B | erase; on a rest right after a note: a tie (hold) |
| L / R | pattern A..D |
| START | loop the pattern from the cursor ("Play from here"; again: stop) |
| SELECT | copy / paste pattern or track, clear track, nudge a track a scale step, loop this bar (ST-PLAY) |

Every placed note is previewed on its own channel (`song_preview`).

### PLAY - jam, record, drum pads (ST-PLAY)

`docs/design/STUDIO_PLAN.md` 2.4. The tab after NOTES. The pads play the song's own scale and chords, so there are no wrong
notes; a take is written into the song with the count-in and quantize; the drum pads play and record the kit.

| Pad | Notes mode (default) | Drum mode |
|---|---|---|
| A B L R | scale degrees 1..4 of the song's key and scale, on the lead (or the chosen track), in the octave shown | kick, snare, hat, open hat |
| X Y L2 R2 (PS1, Dreamcast, PC) | degrees 5..8 (the GBA has no such keys: SELECT + LEFT / RIGHT swaps the four pads between notes 1-4 and 5-8) | |
| d-pad Left Down Right Up | the song's four chords (a triad stacked in scale steps on three channels; the roots are the first bass note of each bar of the song, in order, filled up with I IV V vi) | clap, tom, crash, cowbell |
| SELECT + UP / DOWN | octave up / down (-2..+2) | |
| SELECT + A | chord set: the song's / I IV V vi | |
| SELECT + START | record: a count-in of four clicks, then the loop and the take; again (or START) ends the take | the same |
| START | listen: loop the chosen part; again stops | the same |
| SELECT (tap) | Play tools: Notes / Drums, Next track (lead, harmony, bass), Quantize (off, 1/4, 1/8, 1/16), Overdub / Replace, Loop part (whole pattern, bar 1, bar 2), Next pattern, Chord set, and on the GBA Notes 1-4 / 5-8 | |

* **Jam.** A held pad holds its note (`song_jam_on` / `song_jam_off`): the track's own channel and instrument, over the
  song if it plays (tone channel 0 with the lead instrument by default). Several pads: the last one pressed wins, a
  chord uses channels 0, 1 and 2. Drum pads hit once.
* **Record.** The loop is `song_play_range(pattern, from, to)`; the step of a pad press is decided from the player's tick position
  (`song_pos_ticks`, exact with swing: `play_loop_pos`) and the quantize (`play_quantize_step`): off keeps the step you were in,
  1/16 rounds to the nearest step, 1/8 to the nearest even step, 1/4 to the nearest beat; rounding past the end of the loop wraps to its
  start. A pad held for more than a step writes tie cells after the note (not over other notes). Chords record their root.
  **A take is one undo step**: the track row is touched once (`ed_touch`) when the take starts and committed when it ends, however
  many loops it ran. SELECT + B during a take takes back what was recorded so far and the take goes on. Overdub keeps the old
  notes; Replace clears the looped part when the loop starts, and a Replace take that recorded nothing is undone again.
* **Where the state lives.** 64 bytes of the shared scene scratch (`st_tab_state`; on the GBA inside the studio's bake block, whose
  pixel pool is 64 bytes smaller), reset at every visit; nothing is `.bss`. Leaving the tab (or the studio) ends the take, lets
  go of the pads and stops the loop.
* **Pads vs. global keys.** On the PLAY tab L / R, B and (PS1 / Dreamcast / PC) Y, L2, R2 are pads: tabs are SELECT + L / R, undo is
  SELECT + B, leaving the studio is the FILE tab.
* **Loop while editing.** NOTES > SELECT tools > "Loop this bar" loops the bar under the cursor (`song_play_range`); edits are
  heard on the next pass; START stops it as before.

### ORDER - arrange
The order list (1..16 passes of patterns A..D), each row showing the lyric line
sung over it, and a timeline of coloured blocks with a walking playhead plus
the total length. LEFT / RIGHT change a pass's pattern, a tap of A inserts a
copy below, hold A + UP / DOWN carries a pass, SELECT: insert, delete, move,
repeat on/off. START plays from the selected pass.

### LYRICS
Eight lines of up to 27 characters (ASCII; rude words are swapped for cute
ones). **Line `i` is sung over order position `i`** (positions 8..15 repeat
lines 0..7); an empty line is an instrumental pass. Each lead-track note onset
of that pass sings the next syllable.

* The syllable split is shown live in alternating colours (vowel groups;
  silent final *e*, *-es*, *-ed*; sung *-le* / *-les*; vowel pairs sung apart
  like *pi-a-no*, *ne-on*; a `-` forces a split).
* A fit badge compares syllables with the onsets of the line's pattern: a
  heart when it fits, `+n` (red) when there are too many syllables, `-n`
  (blue) when notes would go unsung.
* The detail strip lays the syllables under the pattern's onsets and shows who
  sings the line (at 320x240 with their chibis).
* LEFT / RIGHT choose **who sings the line**: a solo, a duet, a section
  (sopranos, altos, tenors, basses), a half of the choir (call and response)
  or the full choir - stored in `line_singers`.
* **Fit this line** (SELECT tools; it was START until ST-EASY, START plays now) auto-fits the line: too few words
  get "la"s (or an "oh"), and if there is no room left the last notes merge into long ones (only when no
  other line uses the pattern); too many words split long notes into new
  onsets. SELECT: sing this line, auto-fit all, share out parts, write lyrics
  for me, clear the line, fit this line. START plays from the pass that sings the line.

### CHOIR
Up to 25 citizens on a 9/8/8 grid, each tile tinted by voice section with an
S/A/T/B badge. Sections are automatic from the voice pitch slider (>= 22
soprano, >= 15 alto, >= 8 tenor, else bass) and editable (A on a singer, "Change section", cycles
auto -> S -> A -> T -> B; START plays the song). A on a singer: hear their voice (they sing the first
sung line alone), change singer (the citizen picker), change section, remove.
SELECT **quick fills**: friends of the selected singer, a Hillvile choir, a
Hellvile choir, both districts, random choir, share out parts, sort by section,
clear. The top row picks the stage arrangement: risers, semicircle,
sections, pyramid (unlockable).

### PERFORM
The poster shows the bill; A raises the curtain. The show is full screen:
velvet curtains, a back wall of LED hearts rippling on the beat, sweeping
spotlights and a follow spot (and a star) on the line's lead voice, the whole
choir on its arrangement, every assigned singer bopping and opening their
mouth on each syllable with notes floating up, section glows under whoever
has the line, a conductor (Father Fuzz, or you if he sings or is missing)
waving a baton in 4/4, a crowd of citizens in the dark with glowsticks,
karaoke lyrics (sung syllables gold, the current one pink and bouncing) with
the part label ("Altos (6)", "Full choir (16)" on the GBA), and an applause meter.
Afterwards: stars, applause, an **ENCORE!!** at 80%+, and every singer gains
happiness (+6..+18) and social, and singers next to each other on the risers
grow a little closer. SELECT on the poster hands the song to the Concert Hall
stage when that module offers it (`concert_stage_song`).

### FILE
The song book (6 pages: save here / open / favourite / delete, with
confirmations), **New song...** (the Start card, with a confirmation when the song is unsaved), **templates**,
**Inspire me!**, undo and redo (with the number of steps), the **Studio level** and leave. The list is 13 rows and
scrolls (`studio.ftop`) when the screen is too short (240x160 shows 11). Saving into the book is kept in memory and
written with the game save (the `'SONG'` chunk).

## Function ledger (ST-PLAY: the PLAY tab and its tools)

One row per control: what it says, what it does, the feedback, whether it can be undone, the test. "Pad" rows are the
PS1 / Dreamcast / PC layout; the GBA has the first four pads only.

| Control | What it does | Feedback | Undo | Tested by |
|---|---|---|---|---|
| A B L R (X Y L2 R2) | hold scale degree 1..4 (5..8) of the track in the octave shown | the note sounds while held; the pad lights; its name is on the pad | (nothing is written) | `studio_play_pads_sound_the_songs_notes_and_chords`, `studio_play_jam_stays_in_the_scale` |
| d-pad (notes mode) | hold the chord (triad on 3 channels) | the chord sounds; the chord's name on the pad | (nothing is written) | `studio_play_pads_sound_the_songs_notes_and_chords`, `studio_play_chords_come_from_the_songs_bass` |
| pads (drum mode) | hit kick, snare, hat, open hat, clap, tom, crash, cowbell | the drum sounds; the pad's icon and name | (nothing is written) | `studio_play_drum_pads_hit_the_kit` |
| SELECT + UP / DOWN | octave +1 / -1, from -2 to +2 | "Octave +1"; the pad names change; at the end "Highest / Lowest octave" and a buzz | not a song edit | `studio_play_select_shift_octave_chords_and_limits` |
| SELECT + A | chord set: the song's / I IV V vi | "Chords: I IV V vi"; the chord names change | not a song edit | `studio_play_select_shift_octave_chords_and_limits`, `studio_play_menu_items_do_what_they_say` |
| SELECT + LEFT / RIGHT (GBA) | pads A B L R play notes 1-4 / 5-8 | "Pads A B L R: notes 5-8"; the pad names change; the header says "Notes 5-8" | not a song edit | the same code as menu item 7 (a 4-pad build only: the host has 8 pads) |
| START | loop the chosen part (the pattern or a bar); again: stop | "Looping bar 1 of A" / "Stopped"; the playhead runs on the ruler | not a song edit | `studio_play_count_in_can_be_stopped_and_start_loops_the_part` |
| SELECT + START | record a take: count-in of four clicks, then the loop; again: end the take | "Recording Lead: count-in"; "Count n"; the red REC; at the end "Take saved: n notes" / "No notes: nothing changed" | **one undo step per take** (SELECT + B) | `studio_play_record_quantizes_and_a_take_is_one_undo_step`, `studio_play_record_then_play_back_gives_the_recorded_notes`, `studio_play_a_held_pad_writes_tie_cells`, `studio_play_drums_record_into_the_drum_track` |
| Quantize (menu) | off / 1/4 / 1/8 / 1/16: where a note lands | "Quantize: 1/8"; the header shows it | not a song edit | `studio_play_quantize_table`, `studio_play_quantize_off_and_eighths_change_where_a_note_lands` |
| Overdub / Replace (menu) | a take keeps the old notes / clears the looped part when it starts | "Overdub: keeps the old notes" / "Replace: a take clears the loop" | the take is one undo step; an empty Replace take is undone at once | `studio_play_replace_clears_the_loop_and_an_empty_take_changes_nothing` |
| Notes / Drums (menu) | the pads play and record the kit instead of the scale | "Drum pads: A B L R + d-pad"; the pads become drum icons; the header says "Drums" | not a song edit | `studio_play_drum_pads_hit_the_kit` |
| Next track (menu) | lead -> harmony -> bass: where the pads sound and a take is written | "Pads play and record Harmony"; the header pill changes colour | not a song edit | `studio_play_menu_items_do_what_they_say`, `studio_play_chords_and_other_tracks_record` |
| Loop part (menu) | whole pattern -> bar 1 -> bar 2: what START and a take loop | "Loop: bar 1"; the ruler marks the range | not a song edit | `studio_play_loop_part_limits_the_take_to_a_bar` |
| Next pattern (menu) | pattern A -> D: which pattern is played and written (shared with NOTES) | "Pattern B"; the pattern pill | not a song edit | `studio_play_menu_items_do_what_they_say` |
| Chord set (menu) | the same as SELECT + A | as above | | as above |
| SELECT + B during a take | undo (global) | "Undo! (n left)"; the take goes on | it is the undo | `studio_play_undo_during_a_take_takes_back_what_was_recorded_so_far` |
| SELECT + L / R, leaving the tab | the take ends, the pads let go, the loop stops | "Take saved" | the take is one undo step | `studio_play_leaving_the_tab_ends_the_take_and_stops_the_sound` |
| NOTES > "Loop this bar" | loop the bar under the cursor while editing | "Looping bar 2 of A" | not a song edit | `studio_notes_loop_this_bar_loops_the_bar_under_the_cursor_and_hears_edits` |

Not in this package (ST-EASY / ST-TOOLS own them): the "Advanced" level switch (so Replace is always offered), the cursor line,
the redo, the note grid, per-track velocity.

## Templates

| # | Template | Genre | Notes |
|---|---|---|---|
| 0 | Sugar Rush | Bubblegum Pop | cute pop |
| 1 | Beep Beep Love | Chiptune | tamagotchi chiptune, loops |
| 2 | Plushie Lullaby | Lullabies | |
| 3 | Mosh Pit Picnic | Hardcore Punk | |
| 4 | Doom Cupcakes | Doom Metal | |
| 5 | Organ of Gloom | Goth Rock | gothic organ |
| 6 | Hellvile Flow | Lo-fi Beats | hip-hop beat, swing |
| 7 | Park United! | Folk | **choir**: festival anthem, the Mayor + both districts, Hillvile calls, Hellvile answers, all on the chorus |
| 8 | Hymn of Cuddles | Hymns | **choir**: Father Fuzz and the church folk of the Church of St. Cuddles, sections arrangement |
| 9..13 | Yo Ho Hellvile, Neon Nightmare, Cat Cafe Jazz, Disco Inferno, Forge of Clank | shanty, synthwave, jazz, disco, industrial | unlockable |

A template is ~70 bytes of settings plus its words; the music is composed from
it (so every line fits its notes exactly - a test checks it). On an empty
island the choir pieces fall back to you as the singer.

## The composer and Inspire me

`studio_compose(song, genre, seed)` writes four patterns (A verse, B chorus a
little higher with a crash on the downbeat, C bridge = the verse motif
inverted on a new progression, D intro = the chorus hook thinned out), four
chords per pattern from genre-appropriate progressions, a lead melody built
from a one-bar **motif** (rhythm from a genre-flavoured onset priority list,
contour of scale steps) and its **variation** (same contour moved to the new
chords, a held phrase ending), strong beats on chord tones. Harmony (arpeggio,
pad, stabs, alberti, strum, power fifths, offbeats), bass (roots, octaves,
walking, eighths, long, syncopated) and drums (15 grooves + fills) follow a
16-row genre style table. The number of onsets per pattern follows the words
already in the song. Everything is deterministic from (genre, seed).

**Inspire me!** picks a muse in the citizen picker and writes a song in their
favourite genre with lyrics about them - name, favourite food and hobby, fear,
catchphrase, best friend, crush, district and job (Hellvile citizens get
spookier verses) - and a small choir of the muse and their friends.

## Format notes (song.h)

* Steps are 16ths; a straight step lasts `450 / bpm` ticks at 30 Hz, kept
  exactly with an integer accumulator (`16 * bpm` units per tick, 7200 per
  step). Swing `w` moves `450 * w` units from each odd step to the even one.
  Step 0 sounds at `song_play`; `song_length_ticks` is exact.
* Cells: melodic `0` rest, `1` hold/tie, `2..127` MIDI notes; drums `0` none
  or `SONG_DRUM_*`. On the drum track `wave[3]` is the kit (`SONG_KIT_*`;
  `SONG_KIT_CLASSIC == WAVE_NOISE`).
* Channels: 0 lead, 1 harmony (+ the voice babble channel: the player steps
  aside while a blip sounds and resends afterwards), 2 bass, 3 drums (swept
  noise, or periodic "pitched" noise for toms / cowbell / toy kicks).
* Choir sections: `section[]` nibbles (append-only field), `0` = automatic.
  Use `song_singer_section()`.

### Singing and the choir
Each lead onset sings the next syllable with `audio_voice_char()`. The voice
is **tuned to the note**: the octave nearest the singer's own pitch, then the
PITCH and ACCENT sliders are searched through `audio_voice_freq()` for the
blip closest to the note (intonation off; SPEED and QUALITY stay the
singer's). Long notes keep babbling every 5 ticks. On a choir line the lead
voice rotates through the line's singers per syllable and per retrigger with
a slight pitch spread, and channel 1 doubles the melody a third / fifth /
octave away depending on the sections present, with a chorus wobble and
volume that grow with the number of singers; the lead also gets louder. The
background music is paused while a song plays and restored afterwards.

## Persistence: the 'SONG' chunk
`doomo_save_ext_studio` (save_ext.h) writes the studio stats (13 bytes) and
the used book pages compactly: text by length, notes run-length coded per
used track (runs of rests / holds), choir slots by mask, parts by mask,
sections only when set. An unused studio costs 24 bytes (chunk header included); a full book of
dense songs is ~5.3 KB (the save staging area is 8 KB). Old saves without
the chunk load with an empty book (the chunk's `reset()`), unknown chunks
are skipped, damaged ones reset. `town_new()` resets it.

## Unlockables (ids 240..255, `UL_CAT_MISC`)
Granted from the studio stats: Tiny Pulse (save a song), Buzzy Saw (perform),
Static (20 lyric lines), Toy Box drums (2 songs), Bone Rattle (Inspire me),
Boom 808 (2 shows), Hell Forge (songs in 4 genres), the shanty / disco /
jazz / synthwave / industrial templates (3 songs, 3 shows, 3 inspirations,
an encore, a full 25 choir), the choir pyramid (10+ singers).

## Function ledger (ST-EASY, the detail gate of `design/STUDIO_PLAN.md` 2.5c)
One row per control this package added or changed: what it says, what it does, how it answers, how to take it back,
what tests it. Keys are GBA names; the pads' names are on the control card. "Journal" = SELECT + B undoes it, SELECT
+ A redoes it.

| Control | Says | Does | Feedback | Undo | Tested by |
|---|---|---|---|---|---|
| Start card: Style (L / R, A) | Style | next / previous of the 16 styles | value on the row, cursor blip; the song is not touched until Shuffle / Make it! | nothing changed | `studio_start_card_opens_on_an_empty_song...` |
| Start card: Mood | Mood | Happy / Sweet / Sad / Spooky / Fierce: key mode, key, chord family | value, blip | nothing changed | the same, `studio_make_song_is_deterministic...` (scale per mood) |
| Start card: Tempo | Tempo | Slow / Medium / Fast / Frantic: bpm of the style's range | value and the bpm | nothing changed | `studio_make_song...` (four tempos rise, in range) |
| Start card: Length | Length | 8 / 12 / 16 order passes | value and the bars | nothing changed | `studio_make_song...` (passes, order ends on a chorus) |
| Start card: Shuffle | Shuffle | new seed, same picks, composes and plays | sparkle, toast "Sweet Lo-fi, take 89", the song plays | journal (the first take must have a small song before it; later takes replace the previous take. A song of 768 B or more compressed cannot be remembered, so the history restarts) | `studio_start_card_opens...` |
| Start card: Make it! | Make it! | composes with the current seed, plays, closes the card | sparkle, toast "Made it! START stops the song" | journal | the same (same notes as the last Shuffle; undo twice reaches the blank page) |
| Start card: Inspire me! | Inspire me! | the FILE citizen picker, then an Inspire song | picker, toast "A Jazz song about ...!" | journal; B in the picker cancels | `studio_start_card_inspire_templates_and_skip...` |
| Start card: Templates... | Templates... | FILE tab, template list | the list | B goes back to the FILE list | the same |
| Start card: B | Skip | closes the card, blank song | cancel blip, toast "A blank page. SEL+Up: help" | nothing changed | the same |
| START (every tab but a running show) | Play song / Play from here / Stop | plays the named target; again stops | the bar's verb, the heart and playhead | n/a | `studio_start_plays_from_the_cursor_and_names_its_target` (every tab, both presses, targets) |
| SELECT + UP | (on the bar's help line, the control card) | opens the tab's control card | popup, blip | A / B / START closes | `studio_tab_help_and_control_cards_fit_the_screen` |
| SELECT + A | Redo | puts back the step the last undo took | toast "Redo! (n more)", "Too big to redo", "Nothing to redo" | SELECT + B | `studio_undo_redo_round_trip` |
| FILE: Redo | Redo (n) | the same | the same | SELECT + B | the same, runner `studio_easy` |
| FILE: Undo | Undo (n) | as before | toast "Undone! (n left)" | Redo | the same |
| FILE: Studio level | Studio level: Easy / Advanced | toggles (A, LEFT, RIGHT) | the row, toast "Advanced: more tools soon" | press again | `studio_level_switch_is_kept_in_the_stats_and_the_save` |
| FILE: New song... | New song... | confirms if unsaved, blanks the song, opens the Start card | the card | journal | `studio_start_card_inspire_templates_and_skip...` |
| NOTES: cursor line | (under the grid) | describes the cell: bar, beat, track, note, interval, "off the scale" | text under the grid | n/a | `studio_cursor_cell_is_described_in_one_line_that_fits` (every cell, key and scale fits 240 and 320 px) |
| LYRICS: Fit this line (SELECT tools, was START) | Fit this line | auto-fits the selected line to its notes | toast "Line 2 fits now!" | journal | `studio_fit_one_line_moved_to_the_lyric_tools` |
| CHOIR: START | Play song | plays the song (it cycled a section before: A menu "Change section" has it) | the bar | stop with START | `studio_start_plays...` |
| Tab strip labels | Start / Edit / Arrange / Words / Choir / Show / File, L / R / numbers on the others | names the tabs | the strip | n/a | `studio_tab_help...` (labels set), runner shots |

Not in the ledger because they did not change: every SONG row, the NOTES keys, ORDER, the CHOIR grid, the PERFORM
show, the song book pages.

## Budgets
ST-EASY adds **no `.bss` and no IWRAM** on the GBA (`make -C gba sizes`: `.bss` 254,820 B before and after, `.iwram`
11,272, `.iwram_bss` 4,088): its state (`StEasy`, 448 B: the redo journal 384 B, the Start card's picks, the cursor
line and slot-list caches, the bpm text) is the tail of the studio's baked-shape block in the 12 KB scene scratch
(`BakeSt`, 11,716 B before, 12,164 B now; a compile-time assert keeps it inside `DOOMO_SCENE_SCRATCH`). Where the
shapes are not baked (PS1, native, Dreamcast) `st_easy()` claims the scratch itself (`EZ_TAG`, no static pointer).
The two flags `card` and `help` are bytes of `Studio` that were padding. The level is a spare bit of a stat that is
saved already. ROM: +6.8 KB `.text`, +2.2 KB `.rodata` (GBA, thumb).

`.bss`: the book 6 x 892 B + stats 12 B = 5364 B; everything else 2002 B (the
working song, a 768-byte undo journal - run-length coded byte ranges, 16+
ordinary edits always fit and template loads are undoable too -, UI state,
the player). Const data ~11.6 KB (GBA thumb), code ~60 KB. No malloc, no
floats. The player's cost per tick is bounded: a few steps, <= 4 cached
`plat_tone` calls and at most one voice tuning search (~130
`audio_voice_freq` calls) per syllable.

### GBA speed (MS-P0, measured 2026-11-07, libmgba, game ROM)

`make -C gba profile-studio` (script `gba/harness/scripts/studio_profile.txt`, CI step "Profile the Music
Studio"): the studio opens on the 25-voice "Park United!" choir, every tab is measured over 130 logic ticks
(PERFORM over two shorter windows) and then sampled with `profile`. The budget is 33.3 ms of work per tick for
30 drawn fps and 30.0 logic ticks/s. Deterministic: two runs gave identical figures.

| window | work avg / max ms | drawn fps | logic ticks/s | hottest functions (share of cycles) |
|---|---:|---:|---:|---|
| SONG idle | 35.2 / 35.7 | 24.2 | 30.0 | `vid_rect_narrow` 10%, `vid_rect` 10%, `vid_text` 8%, `ellipse_draw` 5% |
| SONG playing | 36.2 / 41.1 | 22.8 | 30.0 | same four |
| NOTES idle | 33.4 / 35.1 | 29.1 | 30.0 | `vid_rect_narrow` 14%, `vid_rect` 12%, `vid_text` 8%, `vid_rect_full` 6% |
| NOTES playing | 34.7 / 39.3 | 24.7 | 30.0 | same four |
| ORDER | 31.0 / 32.4 | **30.0** | 30.0 | `vid_text` 13%, `vid_rect_narrow` 12%, `vid_rect` 10% |
| **LYRICS** | **60.4 / 60.9** | **12.4** | **22.7** | `song_syllables` 8.7%, `part_mask` 7.9%, `vid_text` 7%, `song_singer_id` 6%, `res_get_voice` 5% |
| **CHOIR** | **52.4 / 54.0** | **15.0** | 30.0 | `vid_blit` 15%, `vid_rect_narrow` 10%, `rr_rows` 5%, `res_get_look` 5% |
| **PERFORM** early | **56.6 / 62.0** | **12.5** | **22.6** | `vid_blit` 21%, `res_get_look` 9.5%, `vid_rect_narrow` 7%, `cache_get` 5%, `draw_show` 3% |
| **PERFORM** later | **56.0 / 62.2** | **13.1** | **22.7** | `vid_blit` 19%, `res_get_look` 8.5%, `vid_rect_narrow` 7%, `cache_get` 5% |

What this says (it corrects two guesses in `MUSIC_STUDIO_ROADMAP.md`):

* The editor tabs are at or just over the budget (SONG / NOTES 33-36 ms, ORDER 31 ms); only ORDER holds 30 fps.
* **LYRICS, CHOIR and PERFORM are about twice over budget** (52-60 ms, 12-15 fps). LYRICS and PERFORM also
  fall under 30 logic ticks/s, so their logic is not keeping up either.
* PERFORM is **not** limited by the checker wall: `plat_checker_m4` is about 2% of it. The cost is the chibis
  (`vid_blit` + `res_get_look` + `cache_get` is about 35%), the same thing that makes CHOIR slow.
* LYRICS spends about 28% in per-frame song derivation (`song_syllables`, `part_mask`, `song_singer_id`,
  `res_get_voice`): the syllable split and singer lookups are redone every frame while the lines have not changed.
* **Correction (2026-11-07, nov07f):** the "sung" PERFORM windows above are the instrumental intro. The first sung line adds ~8.5 ms (50 -> 58 ms) and pushes work past the 3-vblank cliff (50.2 ms), so logic falls to ~22 ticks/s; onset spikes of +3..6 ms are the voice tuning search; the 74-88 ms climb is this step. Breakdown in `docs/MUSIC_STUDIO_ROADMAP.md` "Facts to know before MS-P4".
* Not measured here: PS1 (needs your PCSX-Redux / CI), the PERFORM bows phase, hold-A note edits.

**MS-P3 step 1 (2026-11-07, exact, pixel-identical):** `song_syllables` is memoised (10 entries, 570 B `.bss`; a pure function of the line text)
and `studio_part_of` keeps its last answer per line (keyed on the singers, their sections and the line's mask; reset on entering the studio).
LYRICS on the same script: **60.4 -> 45.3 ms, 12.4 -> 20.2 drawn fps, logic 22.7 -> 30.1 ticks/s**; every other window unchanged (+-0.3 ms).
EWRAM free 21,856 -> 21,152 B.

**MS-P3 step 2 + MS-P4 step 1 (2026-11-07, exact):** `draw_syllables` draws one `plat_text` per same-colour run instead of one `ui_text`
per glyph (plain font style, English; other styles keep the old path); LYRICS shot byte-identical (`cmp` of the harness PNG), LYRICS
**45.3 -> 38.6 ms, 20.3 fps, 30.0 ticks/s**. The chibi look-hash memo in `bodyspr_image` (`blh`) was 32 direct-mapped slots: a 25-voice choir
collided and redid the look + hash for most singers every frame; it is now 40 slots, fully associative (+480 B). CHOIR 52.3 -> 50.8 ms,
PERFORM 56.9 -> 52.1 ms and its logic back to 29.3 ticks/s. EWRAM free 20,672 B. CHOIR / PERFORM are now `vid_blit` (15-21%) plus
`cache_get` / `bodyspr_image` (8-10%) and the rects.

**MS-P2 step 1 (2026-11-07, nov07d, exact):** `ui_backdrop_hole` (ui_draw.c) draws the studio backdrop without what lies wholly inside the opaque
content panel (`backdrop_hole()` in studio.c: SONG / FILE left panel, LYRICS top panel, NOTES / ORDER / CHOIR / STAGE the whole content area; the
template picker keeps the full backdrop). Floating notes under the panel are skipped too. `bodyspr_image` tries the look-hash slot an id used last
(`blh_hint`, 64 B) and `PC_MEMO` is 128 (+128 B EWRAM; EWRAM free 20,544 B). Native shots of every tab over time, 240x160 and 320x240, 98 each: byte-identical.
GBA work avg: SONG idle 35.2 -> 33.1, NOTES idle 33.4 -> 29.6, ORDER 31.0 -> 27.1, LYRICS 38.6 -> 36.5, CHOIR 50.8 -> 46.9, PERFORM 52.1 -> 51.0 ms
(measured in the session before the figures above were re-built; not re-profiled on the final tree). Measured while doing it: backdrop + tab strip +
hint bar ~14 ms of every tab; the 25 CHOIR chibis ~14 ms (CHOIR without them 36.8 ms). See `docs/MUSIC_STUDIO_ROADMAP.md` "Facts to know before MS-P4".

### GBA speed, ST-EASY (2026-11-15, libmgba, `make -C gba profile-studio`, same script, before = `d979c93`)
Work per drawn tick in ms (average / worst; the budget at 30 fps is 33.3); every window is 30.0 drawn fps before and after.
| window | before | after |
|---|---:|---:|
| SONG idle | 31.6 / 33.2 | 31.2 / 32.7 |
| SONG playing | 32.3 / 37.0 | 32.0 / 36.8 |
| NOTES idle | 27.8 / 29.1 | 27.5 / 28.8 |
| NOTES playing | 27.9 / 32.0 | 27.7 / 31.8 |
| ORDER | 25.5 / 26.8 | 25.2 / 26.5 |
| LYRICS | 32.4 / 34.3 | 32.0 / 33.9 |
| CHOIR | 30.5 / 31.9 | 30.5 / 31.7 |
| PERFORM early / later | 22.3 / 22.8, 22.9 / 32.3 | 22.3 / 22.7, 22.8 / 32.2 |
The screens ST-EASY added (`gba/harness/scripts/studio_easy_profile.txt`, work avg / worst, drawn fps): the Start card
21.1 / 22.7, 30.0; the control card over it 13.5 / 13.5, 30.0 (a held frame: only the window repaints); FILE (13 rows,
scrolled) 21.2 / 22.4 at the top and 21.4 / 23.0 at the end, 30.0 (the old FILE tab: 21.7 / 23.1 in `speed2_studio`, now
20.9 / 22.3). The Start card's song, playing on the SONG tab, is the one number under the line: 32.7 / 39.0 ms, 28.8
drawn fps (a Bubblegum Pop song with 12 passes; the same tab with the demo song is 30.0 in the table above): the SONG
tab with a playing song is the tab the plan already lists at 24-29 fps, its cost is the tab's own, ST-EASY neither adds
nor removes any of it. `make -C gba test-speed2 SPEED2=studio SPEED2_GATES=1` passes, every window equal or faster than
on `d979c93` (work ms: file tab 21.7 -> 20.9, templates 29.3 -> 28.9, stage tab 37.8 -> 37.3, still the SPEED
stream's S3-8 item, untouched here).
The cursor line costs about 0.4 ms on NOTES; it is paid back (and 0.3 ms more) by three cached things that were rebuilt
every frame: the tab strip's "110 bpm" text and its two widths (every tab), NOTES' "in Order 2,3,5" list and the
step numbers (a table instead of `%2d`); the inactive tab pills of the narrow strip draw one glyph instead of an icon.
(A first version switched the help line back to the prompt bar on a timer: that cost one 37 ms frame in an idle tab, so
it now switches on the first button press.) PS1 (PCSX-Redux): every editor tab and every new screen 30.13 fps before and after, resident share 0 B, studio overlay load +27 ms (docs/PS1.md "Studio ST-EASY"). Dreamcast (Flycast, lander, after the review fix): `make -C dc` builds; `speed2.sh studio` all nine windows 30.0 ticks/s, worst work 12.6 ms; `boot.txt` passes. The made song playing on the SONG tab is the one window under 30 fps on the GBA (28.8 fps, 32.7 / 39.0 ms): it is the plan's known SONG-playing cost (STUDIO_PLAN section on speed), owner SPEED S3 / ST-ADV, not fixed here.

## INTEGRATION NOTE for the Concert Hall (concert*.c)

1. **Represent generated songs as `DoomoSong`.** Build the song in the shared
   format (`concert_song_export` does this): title, genre, bpm, key, scale,
   instruments, up to 4 patterns + order, 8 lyric lines (line `i` over order
   position `i`), the singers in `singer[]`, who sings what in
   `line_singers[]`, the stage in `choir_layout`, and `seed` != 0 so the
   studio knows it was generated. Run `song_sanitize()` on it.
2. **Play it with the shared player.** `song_play(&song, -1)` (or a single
   singer with `song_play(&song, id)`), call `song_tick()` once per frame,
   `song_stop()` to end early. For visuals: `song_pos_order/step/line/
   syllable()`, `song_pos_singers()` (who is singing now), `song_pos_lead()`
   (the rotating lead voice), `song_beat_age()` (0 on the beat),
   `song_pos_ticks()`, `song_length_ticks()`. The player pauses and restores
   the background music itself.
3. **"Edit in Studio".** Either copy the song into the book and open that page
   - `int slot = song_book_store(&song)` (or write `doomo_song_book[slot]`
   yourself) then `scene_push(&SCENE_STUDIO, slot)` - or, with no free page,
   `studio_edit(&song)`, which opens the studio on a copy that the player
   can save from the FILE tab.
4. **Studio PERFORM -> Concert stage.** Define `int concert_stage_song(const
   DoomoSong *s)` (strong; the studio has a weak default returning 0): push
   `SCENE_CONCERT` with the song and return 1. The pointer stays valid while
   the studio is under you on the scene stack. SELECT on the studio's
   poster calls it.

The concert module currently does 1, 3 and 4 (`concert_song_export`,
`edit_in_studio`, `concert_stage_song`), and plays studio songs through the
shared player.

**Option A (2026-11-07, nov07m, exact):** `vid_blit` in ARM for scale 1 without flip (`video_loops.s`; the rest is `vid_blit_c`). Same script (`studio_profile.txt`): CHOIR **46.5 -> 42.3 ms** (19.9 fps, 30.1 ticks/s), PERFORM intro windows **42.5 / 42.3 -> 36.0 / 35.9 ms**, every editor tab within +-0.1 ms. Sung PERFORM (`studio_sung.txt`): median 47.0 -> 40.5 ms. Details and the exactness checks: `docs/STUDIO_30FPS_PLAN.md` section 7.
