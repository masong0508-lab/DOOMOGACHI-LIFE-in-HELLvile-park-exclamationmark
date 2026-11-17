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
for now; the Studio level switch (FILE) is stored and shown. Advanced is ST-ADV's: see "ST-ADV screens" below.

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
  rides in the existing `'SONG'` chunk with no new save tag; an old save reads Easy. On Advanced the ORDER tab becomes the arrangement
  (see "ST-ADV screens"), the Start card also offers Epic, and FILE shows the fold line.

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
| SELECT (tap) | Play tools: Notes / Drums, Next track (lead, harmony, bass), Quantize (off, 1/4, 1/8, 1/16), Overdub / Replace, Loop part (whole pattern, bar 1, bar 2), Next pattern, Chord set, and Record a take (on the GBA: Notes 1-4 / 5-8 instead; there SELECT+START records) | |

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
| SELECT + START (Dreamcast: SELECT, Tools, "Record a take", because the pad makes SELECT from START+Y) | record a take: count-in of four clicks, then the loop; again: end the take | "Recording Lead: count-in"; "Count n"; the red REC; at the end "Take saved: n notes" / "No notes: nothing changed" | **one undo step per take** (SELECT + B) | `studio_play_record_quantizes_and_a_take_is_one_undo_step`, `studio_play_record_then_play_back_gives_the_recorded_notes`, `studio_play_record_from_the_menu_alone_for_a_pad_without_select_start`, `studio_play_a_held_pad_writes_tie_cells`, `studio_play_drums_record_into_the_drum_track` |
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

## ST-TOOLS: range tools and the seven helper tools (2026-11-16)

Where: NOTES, SELECT tap, "Pattern tools" now has 10 items. Item 9 "Range tools..." and item 10 "Helper tools..." open
a second menu. The work is `core/src/studio_tools.c` (pure functions on a `DoomoSong`, tests `test_studio_tools.c`);
the tabs only call them. Every tool is tried first: if it would change nothing, no undo step is made and the bar says
"Nothing to change there". If it changes something, it is exactly one undo step (SELECT + B) and the grid or the
song card shows the result. The classic 892-byte song is unchanged, so old songs and saves load as before.

**Range** = the steps from the marked start to the cursor on the track under the cursor, or, with no mark, the
bar under the cursor. "Mark the start" toggles the mark (a mint bar left of the steps shows the range).

| Control | What it does | Feedback | Undo | Tested by |
|---|---|---|---|---|
| Range: Mark the start | sets / clears the start step of the range | "Start marked: step 3..." / "Mark cleared: the bar is used"; mint bar on the grid | not a song edit | `tools_mark_makes_the_range_the_marked_steps`, `tools_range_is_the_mark_to_the_cursor_or_the_bar` |
| Range: Up / Down a scale step | moves the notes of the range along the song's scale (ties stay) | "Up a scale step"; the first note sounds | one step | `tools_shift_moves_scale_steps_and_octaves_only_inside_the_range`, `tools_shift_stays_in_the_scale_of_the_song` |
| Range: Up / Down an octave | +/- 12; a note that would leave the keyboard stays | "Up an octave"; the first note sounds | one step | `tools_range_octave_is_one_undo_step_and_nothing_to_change_makes_none` |
| Range: Repeat to the end | copies the range again and again to the end of the pattern | "Repeated to the end" | one step | `tools_repeat_fills_to_the_end_and_refuses_when_nothing_follows` |
| Range: Reverse | reverses the order of the notes; a long note stays long (drums mirror) | "Reversed" | one step | `tools_reverse_keeps_long_notes_long` |
| Range: Twice as fast / Half as fast | halves / doubles the lengths of the range | "Twice as fast" / "Half as fast" | one step | `tools_stretch_halves_and_doubles` |
| Range: Clear range | rests for the range | "Range cleared" | one step | `tools_clear_range_clears_only_the_range` |
| Helper: Suggest a chord | four next chords for the half bar under the cursor from the key and the chord before it (the tonic starts a song); the pick writes the root in the bass and the third in the harmony | the menu names them ("C", "F", "G", "Am"); "Chord F in half bar 2"; the bass note sounds | one step | `tools_chord_suggester_follows_the_chord_before_and_stays_diatonic`, `tools_chord_write_puts_root_and_third_and_names_them`, `tools_chord_suggester_menu_writes_the_chosen_chord_and_undoes` |
| Helper: Suggest a phrase | four one-bar lead phrases on the bar's chords (the composer's motif code; the same song gives the same four); the pick writes one into the bar under the cursor | "Phrase written to the lead"; the first note sounds | one step | `tools_melody_helper_gives_four_in_scale_phrases_that_differ_and_repeat`, `tools_phrase_groove_and_harmony_menus_write_and_undo` |
| Helper: Pick a groove | the 15 composer grooves by name (page 2 "More grooves...", plus Silence), each with its own fill, into the pattern's drum track | "Pop drums in A"; the first hit sounds | one step | `tools_groove_picker_writes_each_groove_and_the_fill_is_optional`, `tools_phrase_groove_and_harmony_menus_write_and_undo` |
| Helper: Harmonize the lead | a line a third / fifth / octave below the lead, into the harmony track (all in the song's scale) | "Harmony: a third below"; "Write a lead melody first" when there is no lead | one step | `tools_harmonize_writes_a_scale_line_under_the_lead`, `tools_harmonize_without_a_lead_says_so_and_changes_nothing` |
| Helper (and SONG tools): Balance the mix | track volumes by role: lead on top, then bass, drums, harmony; empty tracks keep theirs | "Mix balanced by role" / "Balanced: the lead is heard" / "The mix is already balanced" | one step | `tools_mix_helper_balances_by_role_and_flags_a_track_that_hides_the_lead`, `tools_balance_the_mix_from_the_song_tools_and_the_helper_menu` |
| ARRANGE tools: Copy as new part / a step higher / other drums (section copier) | copies the selected pass's pattern into a free pattern (shifted a scale step, or with another groove) and adds a pass using it below | "Part C copied below"; the selection moves to the new pass; when all four parts are used the copy replaces the next one and says so | one step (the part and the pass together) | `tools_section_copier_copies_shifts_and_swaps_drums`, `tools_section_copier_in_arrange_adds_a_pass_with_a_free_part_in_one_undo_step` |
| CHOIR quick fill: Harmony: thirds / fifths (choir auto-harmony) | writes the harmony line under the lead of every pattern the song plays | "Harmony: thirds under the lead" | one step | `tools_choir_auto_harmony_writes_thirds_for_every_used_pattern_in_one_step` |
| WORDS tools: How does it fit? (syllable fitter report; "Fit this line" / "Auto-fit all lines" were there) | says how many words sit on how many notes: a fit / la la fills / too many | "2 words, 2 notes: a fit!" | a report: nothing changes | `tools_lyric_fit_report_says_words_against_notes`, `tools_fit_status_says_how_words_sit_on_the_notes` |

A bug found on the way: on ARRANGE the A press that chose a menu item inserted a copy when it was released. Only a tap
that began on the tab inserts now (`studio.o_a`); covered by the section copier test.

### ST-TOOLS, second pass (2026-11-16): review, help lines, the drum editor and the chord track

**Review of the first three commits.** Found and fixed, each with a test: the mix helper drifted (every press on a song
without a lead lowered the volumes a little more, so "already balanced" never came): the lead, or when there is none the
bass / drums / harmony in that order, is now the fixed measure (`tools_mix_balance_is_stable_and_never_moves_the_track_it_is_measured_by`);
the chord suggester took "the half bar before" from the pattern letter before, not from the order list, and a pentatonic song
could be offered a sixth chord it does not have (`tools_chord_suggester_starts_a_part_after_the_part_before_it_in_the_order`);
"Start marked: step 32. Move, then pick" was longer than the 31 glyphs of the message bubble and was cut off (now "Start at step 32.
Now move"; every message of every tool is checked for 31 glyphs and the 240 px screen by `msg_fits` inside the tests' menu helper);
"1 words" and "1 notes" (plurals); a mark set in one pattern stayed in the next one (L / R clears it); `tl_phrase_write` formed a
pointer from a bad bar before testing it; "Track up / down a step" (Pattern tools) always made an undo step and said nothing: it
goes through the same try-first machinery as the range tools, says "Track up a scale step" / "Nothing to change there" and sounds
the first note; copy / paste / clear now sound (blip / sparkle). Checked and found right: reverse keeps long notes whole, repeat
and half speed stop at the pattern end, transposing past the keyboard (24..108) leaves a note alone instead of wrapping, an empty
song makes no step with any tool (`tools_every_tool_leaves_an_empty_song_alone_and_clamps_a_range_past_the_end`,
`tools_a_full_song_transposes_to_the_keyboard_edge_and_stops_there`), and every range tool on every track is one undo step that
puts back the exact bytes and redoes (`tools_every_range_tool_on_every_track_is_one_undo_step_and_undoes_to_the_exact_bytes`).
"Half as fast" writes over the steps after the range (a note twice as long needs them); it says so in its help line.

**A help line for every item.** A menu can give each item one line (`st_popup_help`, 36 glyphs at most): the card's last line
says what the highlighted item does, so it is part of the card and a held popup frame repaints it. The Pattern tools, Range
tools, Helper tools, Next chord, All chords, Phrase, Groove (both pages), Harmony, Song tools, Arrange, Lyric tools, Quick fill,
Drum tools and Chord tools menus have them (`tools_every_menu_item_has_a_help_line_that_fits_the_card` opens each and measures
every line against 240 px). The ten item menus draw their rows 11 px apart, so the card and its help line fit 160 px. Opening a
menu clears the message bubble (the card covers it, and with the bubble up the popup frame cannot be held: a full frame of
the chord track under a card was 55 ms on the GBA).

**Drum editor** (Helper tools, "Drum editor"): the drums of one pattern as eight lanes (Kick, Snare, Hat, Open hat, Clap,
Tom, Crash, Cowbell) by 32 steps, the beat blocks shaded, the bar line marked, the playing step lit.
The song format has one drums cell per step, so a step holds one drum: a new drum replaces the one there (this is said on the
control card). It is a screen of the NOTES tab on the same song bytes, so old songs and saves are untouched.

| Control | What it does | Feedback | Undo | Tested by |
|---|---|---|---|---|
| Drum editor: d-pad | up / down the lane, left / right the step (they wrap) | cursor blip, the frame moves, the line under the grid names "Step 9: Hat" | not a song edit | `tools_drum_editor_puts_one_drum_per_step_in_its_lane_and_undoes_each_press` |
| Drum editor: A | puts the lane's drum on the step; on the same drum again takes it off; another drum there is replaced | the drum sounds (a blip when it goes), the block appears | one step per press | the same |
| Drum editor: L / R | the other pattern | cursor blip, the pills | not a song edit | the same |
| Drum editor: B | back to the NOTES grid | cancel blip | n/a | the same |
| Drum editor: SELECT, Pick a groove | the groove menu of the helper tools | as the groove picker | one step | `tools_drum_editor_menu_grooves_clears_and_repeats_with_one_step_each` |
| Drum editor: Clear this lane | takes every hit of the lane under the cursor out | "Lane cleared" / "Nothing to change there" | one step | the same |
| Drum editor: Clear all drums | the drums of the pattern | "Drums cleared" | one step | the same |
| Drum editor: Repeat bar 1 in bar 2 | copies bar 1's beat over bar 2 | "Bar 1 repeated in bar 2", the first hit sounds | one step | the same |
| Drum editor: Loop this bar | the bar under the cursor loops | "Looping bar 1 of A" | not a song edit | the same |

**Chord track** (Helper tools, "Chord track"): every pattern A to D as a row, the four half bars of each as cells, each cell the
chord's name in the key and its number ("Am  VI"), "-" where nothing plays. The chord is what the chord suggester writes (the bass
root and the harmony third of the half bar), so it needs no new song bytes; the chord lane of the plan as a fifth track and
"follow chords" arrangement styles are ST-ADV's (`'SNG2'`).

| Control | What it does | Feedback | Undo | Tested by |
|---|---|---|---|---|
| Chord track: d-pad / L / R | left / right the half bar, up / down (or L / R) the pattern | cursor blip; the chord under the cursor sounds; the line under the grid says "B, half bar 2: Am" | not a song edit | `tools_chord_track_shows_every_part_picks_a_chord_and_undoes` |
| Chord track: A | the Next chord menu: four chords that fit, "All chords..." (every chord of the scale, named in the key), "Clear this chord" | the names; the chord sounds; "Chord G in half bar 2" / "G is already there" | one step per chord; none when it is already there | the same |
| Chord track: SELECT, Pick a chord | the same menu | | | `tools_chord_track_menu_repeats_clears_and_hears_the_part` |
| Chord track: Clear this chord | the bass and harmony rest in the half bar | "Chord cleared" / "Nothing to change there" | one step | the same |
| Chord track: Repeat to the end | the chord is written into the half bars after it | "Chord repeated to the end", it sounds | one step | the same |
| Chord track: Hear this part | the pattern loops | "Looping part A" | not a song edit | the same |
| Chord track: B | back to the NOTES grid | | n/a | the same |

Pure functions behind them (`studio_tools.c`): `tl_drum_toggle`, `tl_drum_lane_clear`, `tl_chord_clear`, `tl_chord_repeat`,
`tl_chord_count`; screens and menus in `studio_grid.c`. Tests: `tools_drum_toggle_replaces_the_drum_on_a_step_and_lane_clear_takes_one_drum_out`,
`tools_chord_clear_and_repeat_work_on_half_bars`, `tools_the_new_screens_draw_without_overflowing_a_narrow_row`.

**Instrument, volume, swing.** Already in the SONG tab (one instrument and one volume per track, one swing for the song, the
drum kit); their rows say what they do on the control card and the bar. **Not here, and why:** a per-track envelope, a per-track
pan and a per-track swing need song bytes the classic 892-byte song does not have (the volume byte's top bits would clash with
every reader of it, the saved books and the concert players), and the player would need to read them: both are `'SNG2'` and the
player of ST-ADV, which the plan puts there. A true lane mute (the drum editor's lanes share one cell, so a muted lane cannot
be kept) is the same: "Clear this lane" is what the editor offers. The note grid as a piano roll and a rectangle selection are
the same plan item; the range tools and the marked range cover the editing they were for.

Not in this package (listed, not hidden): the note grid as a piano roll, a rectangle selection across tracks, copy / paste
of a range between patterns (the pattern / track copy and paste are there), per-track pan and envelope presets, per-track
swing and a fifth chord lane. They need new song bytes (the `'SNG2'` chunk of `design/STUDIO_PLAN.md` section 3, ST-ADV)
or a new screen. The chord suggester writes into the bass and harmony tracks instead of a chord lane, so old songs and
the old player keep working. The helper menus are the full version on every platform (the table of 2.5b allowed
lighter versions: none was needed, the tools add about 6 KB of code and no resident data beyond 8 bytes of `Studio`).

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

## ST-ADV: the Advanced song extension (core only, 2026-11-15)
Code: `core/include/doomo/song_ext.h`, `core/src/song_ext.c`, `song_play_ext()` in `song_player.c`. Tests: `core/tests/test_song_ext.c`.
The ORDER tab uses it on Advanced (see "ST-ADV screens" below).

A `SongExt` rides beside the classic `DoomoSong`, which is not changed. A song with no `SongExt` is classic and plays as before
(`song_play(s, singer)` is `song_play_ext(s, NULL, singer)`). A `SongExt` is 2,176 bytes in RAM.
- **Sections:** up to 16. Each has a name (six defaults: intro, verse, pre-chorus, chorus, bridge, outro, or a custom id),
  1..8 patterns, a key shift (-12..+12), a tempo (50..200 %), a fade (none / in / out) and a brightness (stored, not played yet).
- **Arrangement:** up to 64 section entries, so up to 512 passes of 2 bars. `songx_len`, `songx_at` and `songx_ticks` give the
  length, the pattern of a pass and the exact time (tempo per section).
- **Patterns and cells:** up to 32 patterns, up to 8 tracks. Patterns 0..3 on tracks 0..3 are the classic song's own cells.
  Every other cell lives in a pool of 48 blocks of 32 steps; an empty cell costs nothing, a full pool refuses the edit.
- **Folding:** a console plays `songx_tracks_max()` tracks (4 for now). Tracks 5 and 6 are mixed into the harmony lane, 7 into
  the bass, 8 into the drums, only where that lane is empty at the step. `songx_folded()` gives the message for FILE.
- **Lyrics:** a section names the lyric line of each of its passes (`line[pass mod 4]`, 0xFF = instrumental). The lines and their
  singers are the classic song's (8 lines, `line_singers`).
- **Save:** one encoded page per book slot (at most 1,536 bytes, run-length coded) is written as a tail of the `'SONG'` chunk
  (mask, then length and bytes per page). The chunk version stays 1: an old chunk has no tail and loads, an old build ignores the
  tail. A damaged tail is dropped and the songs stay. A classic save, a delete and a Concert Hall export drop the slot's page.
- **Memory:** `SONGX_BOOK_PAGES` is 6 on the host and the Dreamcast and 0 on the GBA and PS1, until the coordinator grants
  resident bytes (plan section 4). Nothing was added to the GBA's `.bss`; the GBA and PS1 builds were not run.

Done since this package (see the sections below): the Advanced screens (ORDER arrangement, part view, Mix, Expert, Note grid with Box,
Velocity bar, Lyric lines), the 32 / 64 bar Start card (Epic), the FILE fold message, Sound / Brightness / Swing playback, free notes.
Not done: a working copy of the `SongExt` in the scene scratch (it lives in `studio.xw`),
patterns of 3 and 4 bars.

## ST-ADV: Epic length and the FILE fold line (2026-11-16)
Code: `studio_start.c`, `studio_adv.c` (`st_adv_epic`, `st_adv_folded`), `studio_file.c`. Tests: `adv_start_card_offers_epic_only_on_advanced`,
`adv_epic_make_lays_out_64_bars_and_undo_takes_it_back_in_two_steps`, `adv_epic_on_easy_falls_back_to_long`, `adv_file_fold_message_only_when_tracks_exceed_the_console`.
- **Epic:** on Advanced (host and Dreamcast builds) the Start card's Length also offers Epic, 64 bars: the made song (16 passes) is laid out as an
  8-entry arrangement of Verse, Chorus, Verse, Chorus, Bridge, Chorus, Chorus, Outro (32 passes). It is a second undo step after the take. On Easy
  or on the GBA / PS1 it falls back to Long.
- **FILE:** when the open arrangement has more tracks than the console plays, the FILE tab shows "Extra tracks are mixed in" (full text from `songx_folded`).
- **LYRICS on an arrangement:** with the arrangement open, `studio_line_pos` / `studio_line_pat` (`studio_gen.c`) answer from the arrangement (`st_adv_line_pass`, `st_adv_line_pattern`): the LYRICS tab's fit badges, onsets strip, pattern chip, autofit, auto parts and the one-line preview all follow the pass the arrangement sings the line on. Test: `adv_lyric_lines_follow_the_arrangement`. Classic songs are unchanged.
- Still open: pan playback (built since), more than 8 lyric lines (built since: see "ST-ADV: Lyric lines 9..16").

## ST-ADV screens: the Advanced ORDER tab (2026-11-16, host and Dreamcast builds)
Code: `core/src/studio_adv.c`; helpers in `song_ext.c`; `song_play_ext_at()` in the player. Where `SONGX_BOOK_PAGES` is 0 (GBA, PS1)
`STUDIO_ADV` is 0, `studio.xw` does not exist and `studio_adv.c` compiles only no-op stubs: the ORDER tab stays classic there.
- **Opening:** with the Studio level on Advanced, the first time the ORDER tab opens the classic order becomes an arrangement
  (`songx_from_order`: up to 4 parts of 4 passes: Verse, Chorus, Bridge, Outro; the same patterns, lyric lines and length).
  It plays the same until edited. Opening is not an edit: the song stays clean and the help line stays.
- **Working copy:** `studio.xw` (a `SongExt`, 2,176 B in `.bss` on the host) sits right behind `studio.song`; the undo journal
  reaches both (`STUDIO_JBYTES`), so every edit is one undo / redo step (nudges of one setting join into one).
- **Arrangement list:** one row per entry: part chip, its passes (A..D), key, tempo %, fade; a timeline under it. LEFT / RIGHT
  picks another part for the entry, hold A + UP / DOWN carries it, A opens the part. SELECT tools: Edit this part, Insert a copy,
  Make it unique, New part, Delete, Key change, Tempo change, Fade, Last chorus up, Repeat on/off.
- **Part view:** name, key (-12..+12), tempo (50..200 %, steps of 5), fade, and 1..8 passes (a pass is a pattern A..D). A part
  used by several entries changes everywhere ("used 3 times"); Make it unique gives one entry its own copy. B goes back.
- **Play:** START plays the arrangement from the cursor's pass (`st_adv_play`).
- **PERFORM and the Concert Hall:** the stage show plays the arrangement (`st_adv_play(0)`), ends at its exact length (`st_adv_ticks`, tempo per part) and scores line fit over the pattern the arrangement sings the line on (`st_adv_line_pattern`). The Concert Hall button hands it over with `concert_stage_song_x(s, x)` (`cross-stream:` a small hook in `concert_scene.c`; `concert_stage_song(s)` is unchanged). Three PERFORM tests in `test_studio_tools.c`.
- **Save and load:** saving to a slot keeps the page (`songx_book_put`); if it does not fit, the song is saved classic and says so.
  Opening a slot loads its page. Make it! / Shuffle, templates, Inspire me! and New drop the arrangement in the same undo step.
- **Mix view (2026-11-16):** in the part view, SELECT tools, "Mix and sound...". One row per track (4, up to 8) with Pan (left / middle / right),
  Sound (soft / punchy / long / pluck) and Swing (song / 1..8), and a last row for the part's Brightness (as set / 1..3). UP / DOWN pick the row,
  A moves the column, LEFT / RIGHT change the value, B goes back. Tracks the console folds say "mixed in". Each edit is one undo step (nudges join);
  the settings are saved with the page. Sound and Brightness are played (see "Mix playback"); pan and swing are stored only (the screen says so).
  The undo journal range now covers the track settings (`META` in `studio_adv.c`). Tests: `adv_mix_view_edits_pan_sound_swing_and_brightness_one_undo_step_each`,
  `adv_mix_settings_are_saved_with_the_page`. The Arrange popup is full (10 items is the popup limit), so Mix is reached from the part view.
- **Expert view (2026-11-16):** in the part view, SELECT tools, "Expert view..." (item 10). The **Free notes** switch (A / LEFT / RIGHT, one undo step, saved in `SongExt.flags`):
  when on, a new note in the NOTES tracker keeps the pitch you last used instead of snapping to the scale (`st_adv_free()`, `studio_edit.c`). Under it: notes per track
  for the patterns the arrangement uses, the notes the whole song plays, and a memory meter (parts / 16, entries / 64, note blocks / 48, encoded save page / 1,536 B).
  B goes back to the part. (The per-note velocity bar came later: see "Velocity bar" below.) Tests: `adv_expert_view_free_notes_switch_is_one_undo_step_and_stops_the_scale_snap`,
  `adv_expert_free_flag_is_saved_with_the_page`. Not run: GBA / PS1 / DC builds.
- **Note grid (2026-11-16):** in the part view, SELECT tools, "Notes, expert...", then the Note grid row (A). A piano roll for one pattern (32 steps) of one track:
  pitch rows (the scale's own notes are the light rows) or, on the drum tracks (4 and 8), the 8 drum lanes. LEFT / RIGHT step, UP / DOWN pitch (a scale step; one semitone
  with Free notes on; a lane on drums), A puts the pitch on the step or takes it off (its ties go with it), **L / R pick the track** (the studio's L / R tab switch is off while
  the grid is open, `st_adv_grid()`), B goes back to Expert. SELECT tools: Next / Previous pattern, New pattern (copy), Tie to the note before (again: off), Clear this track here,
  Octave up / down, Add a track, Take the last track away (only when it is empty), Hear this part. Parts' passes now pick any of the song's patterns (A..Z, a..f; up to 32).
  A pattern past D and tracks 5..8 live in the note pool (48 blocks; "Room n/48" on the screen); an emptied cell gives its block back. Every press is one undo step
  (it journals exactly the bytes it changes, pool included). Code: `studio_adv.c` ("the Note grid"), `songx_pattern_cost` / `songx_pattern_copy` in `song_ext.c`.
  Tests: nine `adv_note_grid_*` in `test_studio_tools.c`. Lyrics fit still reads patterns A..D only (`st_adv_line_pattern` answers -1 for a later one).
  Not run: GBA / PS1 / DC builds, GBA sizes and speed windows for the grid (the GBA keeps 2 Advanced pages, so it compiles there).
- **Box (2026-11-16):** rectangle selection in the Note grid. Grid tools item 10 "Box and paste..." (now "Box and loudness...") opens "Select a box" / "Paste here" / "Hear this part"
  ("Hear this part" moved here; START plays as everywhere). "Select a box" anchors a corner at the cursor; LEFT / RIGHT stretch it over steps, L / R over tracks, and
  the grid shows every track at once (the box view: a row per track, notes as blocks, ties as lines, the box framed). A or SELECT open the box tools: Copy, Cut (copy, empty,
  drop the box), Clear, Octave up / down (pitch tracks only; refuses if one note would leave the keyboard), Paste here, Drop the box. B drops the box (the next B leaves
  the grid). Dropping puts the cursor on the box's top-left corner; Paste puts the copy's top-left corner at the cursor, on any pattern (drop the box, pick another
  pattern, paste). The clipboard is up to 8 tracks x 32 steps (256 B in `.bss`, emptied when a song is opened). A drum row pastes only onto a drum track and a pitch row
  onto a pitch track ("Pasted; 1 track skipped" / "Drums and pitches do not mix"); steps past the pattern end and tracks past the last one are left out. A tie
  needs a note before it: after Clear or Paste, ties that lost their note go too. Paste checks the note pool first ("No room: needs n note blocks"), so it never half-pastes.
  Each command is one undo step. Tests (`test_studio_tools.c`): `adv_box_selects_over_steps_and_tracks_and_b_drops_it_without_leaving_the_grid`,
  `adv_box_copy_paste_moves_a_two_track_phrase_and_one_undo_step_takes_it_back`, `adv_box_cut_and_clear_take_the_ties_of_their_notes_and_undo_puts_them_back`,
  `adv_box_octave_moves_pitch_rows_only_and_refuses_out_of_range`, `adv_box_paste_refuses_drums_on_a_pitch_track_and_an_empty_clipboard`.
  Not run: GBA / PS1 / DC builds, GBA sizes (+259 B `.bss`) and the speed window of the box view. (Later: Louder / Softer in the box tools, see "Velocity bar".)
- **Mix playback (2026-11-16):** the player now uses two Mix settings (`song_player.c`, only with an arrangement; a classic song plays bit-identically). **Sound** (per lane 1..3, track 1..3's
  setting): soft = the classic fall (the default), punchy falls to half in about a third of a second, long rings on, pluck dies away fast; the choir doubling and jam pads keep the classic shape.
  **Brightness** (a part's lead): as set / 1 = 50 % pulse, 2 = 25 %, 3 = 12 % (thinner, brighter). Not played: pan (the players have no stereo for the PSG voices) and per-track swing
  (the step clock is shared; the song's swing still applies). The Mix screen footer now says so (\"Sound and light play. Pan, swing: saved.\"). Cost: +3 B in the player state, a few lines of code
  (PS1 resident share: re-measure). Test: `song_ext_sound_shapes_change_how_a_note_dies_away` (`test_song_ext.c`). Brightness has no test (the native platform does not record the wave).
- **Lyric lines (2026-11-16):** the part view's last row, "Lyric lines  n of m sung  >" (A), opens a screen with one row per pass of the part (the first 4: a part
  names a line for pass k, and pass k + 4 sings the same one). Each row shows the pass, its pattern, the line (`< 3 >`, or `< - >` for instrumental) and the line's words
  cut to fit (\"(no words yet)\" for an empty line). LEFT / RIGHT / A step instrumental, line 1..8 and round again; UP / DOWN pick the pass; B goes back to the part view
  with the cursor on the row. SELECT tools: **Fill from the lyrics** (the next lines that have words on this part's passes, starting after the highest line the other
  parts sing; \"Write some lyrics first\" and no change when the song has no words), **All instrumental**, **Hear this part**. Each change is one undo step (nudges of one
  pass join); the lines were already saved with the page. LYRICS tab, fit badges and PERFORM follow the lines at once (`st_adv_line_pass`). Code: `studio_adv.c` (\"the Lyric
  lines view\"), `studio.aview` 5. Tests (`test_studio_tools.c`): `adv_lyric_view_opens_from_the_part_view_sets_a_line_and_b_goes_back`,
  `adv_lyric_view_a_line_set_here_is_the_one_the_lyrics_tab_finds`, `adv_lyric_view_fill_clear_and_empty_lyrics`, `adv_lyric_lines_are_saved_with_the_page`.
  Not run: GBA / PS1 / DC builds, GBA sizes and the speed window of the new view (ROM only: no new `.bss`). Lines 9..16 were added afterwards (see "ST-ADV: Lyric lines 9..16"). Not built: naming a part's singers here.
- **Swing playback (2026-11-16):** the Mix view's Swing is played (`song_player.c`, only with an arrangement). A track's swing 1..8 is the lateness it wants for its notes on the
  off-beat steps; the step clock already gives the song's own swing, so only what is above it shows (a track at or under the song's swing, or \"song\", plays as before). The wait is
  whole ticks (30 Hz): at 120 bpm swing 1..2 rounds to nothing, 3..5 wait one tick, 6..8 two. Tracks 1..4 (lead, harmony, bass, drums); folded tracks 5..8 follow their lane's
  clock. A waiting note sounds before the next step's work, so nothing is lost at fast tempos. Cost: +8 B of player state (`late[4]`, `lv[4]`; PS1 resident share: re-measure).
  The Mix footer now reads \"Sound, light, swing play. Pan: saved.\" (the players have no stereo for the PSG voices). A classic song plays bit-identically (golden music and
  concert tests pass). Test: `song_ext_track_swing_delays_an_off_beat_note_and_the_song_swing_comes_first`.
- **Velocity bar (2026-11-16):** the per-note loudness of the Expert views. A pitch note has one of four levels: Soft (60 % of its track's volume), Normal (the default,
  100 %), Loud (130 %) and Peak (160 %, never over 15). Code: `song_ext.c` (`songx_vel`, `songx_set_vel`, `songx_vel_pct`), `song_player.c` (`cell_vel`, `lane_on`),
  `studio_adv.c` (Note grid and Box). Tests: `song_ext_velocity_*` (3, `test_song_ext.c`), `adv_velocity_*` (4, `test_studio_tools.c`).
  - **Format:** a cell's byte does not change, so the classic song and every old page stay valid. `SongExt` gets a last field `vel[32][8][8]` (2 bits a step, 2,048 B; zero = every note
    Normal; the code is stored as 0 = Normal so a fresh layer is all Normal). A saved page gets a tail only when some note is not Normal (`SX_FLAG_VEL` in the flags byte, then a u16 count and,
    per pattern / track that has one, one id byte and its 8 level bytes). A page with no accents is byte for byte the old page; a page cut inside the tail keeps the song and drops the accents.
    A copied pattern (New pattern) brings its accents. The undo journal reaches the layer because it covers the whole `SongExt`.
  - **Note grid:** under the roll of a pitch track a **Vel strip** shows a bar for every note, as tall as it is loud (Soft short and dim, Peak full), with the cursor's step marked; the line
    under it names the level of the note at the cursor. Grid tools item 10 is now "Box and loudness...": Select a box, Paste here, Hear this part, **Louder note**, **Softer note**
    (the note at the cursor, one level a press; "No note here", "Already at Peak", "Already Soft", "Drums keep their own level" and no undo step when nothing changes).
    The roll has about 10 px fewer for pitch rows to make room for the strip. A note that goes (taken off, cut, cleared) forgets its loudness, and undo brings both back.
  - **Box:** the box tools are now Copy, Cut, Clear, Octave up, Octave down, **Louder**, **Softer**, Paste here, Drop the box. Louder / Softer move every pitch note in the box one level
    (one undo step). Copy and Cut keep each note's loudness (a 64 B clipboard beside the 256 B of cells) and Paste puts it back with the note. The box view shows Soft as a half block and
    Loud / Peak with a bright cap (1 px / 2 px).
  - **Played:** `do_step` multiplies the lane's volume by the note's level (the lead, harmony and bass lanes; a note folded from tracks 5..7 takes its own track's level; a swung note keeps
    its level while it waits: +4 B of player state, `lvl[4]`). A Normal note is the classic note (test). Drums keep their kit levels (the drum cells are not accented).
  - **Memory:** the working `SongExt` grows from 2,176 to 4,224 B and the box clipboard by 64 B: expected `.bss` +2,112 B on the GBA (about 43 KB EWRAM left), host and Dreamcast the same. Not
    measured here: `make -C gba sizes`, GBA speed windows (the grid draws one more strip), PS1 resident share (`song_player.c` +4 B). The saved page is still at most 1,536 B (a page that does not
    fit is saved classic, as before); the worst-case book chunk in the test output is unchanged at 5,305 B.
- **One-bar patterns (2026-11-16):** a pattern is two bars (32 steps) or one bar (16 steps). Code: `song_ext.c` (`songx_plen`, `songx_set_plen`, `songx_ticks`), `song_player.c` (`advance`),
  `studio_adv.c` (`g_length`). Tests: `song_ext_a_one_bar_pattern_*` (`test_song_ext.c`), `adv_pattern_length_*` (`test_studio_tools.c`). Plan 2.6 asked for 1..4 bars; 3 and 4 bars need
  cells longer than 32 steps (a format change of the pool) and are still open.
  - **Format:** `SongExt.short_mask[4]` (bit p = pattern p is one bar), at the end of the struct (+4 B). The cells do not change: the second bar of a one-bar pattern is kept, just not played,
    so switching back brings it back. A saved page gets a 4-byte tail only when some pattern is one bar (`SX_FLAG_SHORT`, after the velocity tail); a page with none is the old page.
  - **Played:** the pass of a one-bar pattern ends at step 16 (`advance`), so every part, the PERFORM show and the Concert Hall run on the shorter pass; `songx_ticks` sums the real steps of
    every pass (a one-bar pass is half the time). Classic songs and two-bar patterns are unchanged (the same tests pass).
  - **Note grid:** "Box and more..." (grid tools item 10), last item "One bar / two bars" switches the pattern at the cursor (one undo step; the cursor moves inside the first bar, LEFT / RIGHT wrap at
    step 16, the rest of the roll is shaded; the footer says "Pattern A: one bar"). The box view shades it too. Lyric fit and the onsets strip still read all 32 steps.
- **Limits:** 64 entries, 16 parts, 8 passes per part. Removing the last user of a part removes the part.
- **GBA (2026-11-16):** `SONGX_BOOK_PAGES` is 2 on the GBA, so the Advanced ORDER screen runs there too (book slots 0 and 1 keep an arrangement page; other slots save classic and say so). `.bss` +5,264 B (210,548 -> 215,812; about 45 KB EWRAM left), IWRAM unchanged. Speed (`make -C gba test-speed2 SPEED2=studio_adv`): arrangement idle 30.0 fps (work 19.7 ms avg), playing 30.0 (19.3), part view 30.0 (21.0), tools menu 30.0 (20.1), 0 frames over 33 ms. PS1 still 0 pages (stubs).
- **Not done:** patterns of 3 and 4 bars.
- **Tests:** `adv_from_order_plays_like_the_classic_order`, `adv_arrangement_insert_remove_and_entry_passes`,
  `adv_order_tab_opens_the_arrangement_once_and_undo_reaches_it`, `adv_a_take_drops_the_arrangement_in_the_same_undo_step`,
  `adv_the_page_is_saved_with_the_song_and_loads_back`, the `adv_keys_*` journeys and the PERFORM tests (`test_studio_tools.c`).
- **Not run:** GBA, PS1 and Dreamcast builds, `make -C gba sizes`, speed windows for the new screen.

## ST-ADV: Pan playback (2026-11-16; core + GBA / PS1 / DC drivers written, only the host is built and tested)
Code: `plat_tone_pan` (`core/include/doomo/plat_ext.h`, weak default in `plat_weak_ext.c`), `pan_set` / `pan_reset` in `song_player.c`, `gba/src/audio.c`, `ps1/src/spu.c`,
`dc/src/mixer.c` + `audio.c`, the native recorder `nat_tone_pan` (`plat_native.c`). Test: `song_ext_mix_pan_is_sent_to_the_platform_while_the_song_plays_and_reset_when_it_stops`.
- **Contract:** `plat_tone_pan(channel 0..3, pan)`, pan 0 left, 1 middle (the port's own place for the channel), 2 right. Additive: a port that does not define it plays centre.
- **Player:** when a note sounds on a lane the player sends that lane's Mix view pan (`SongExt.trk[lane].pan`) if it differs from what it sent last (a change made in the Mix view
  while the song plays is heard on the next note); `silence_all` (song start, stop) sends the middle back for every lane that was panned. A classic song, a preview and a jam over
  no arrangement never send a pan call (test), so nothing else on the tone channels is moved.
- **GBA:** the PSG has a left / right enable per channel, so the range is left / middle / right and nothing between: `REG_SNDDMGCNT` bits 12..15 (left) and 8..11 (right), the low
  byte (master volume) is kept. One register write per change, interrupts off around it. Not built: `make -C gba` not run here, no listening check.
- **PS1:** left / right voice volumes of the SPU voice: the far side keeps a quarter of the level (still heard on a mono TV). Not built, not listened to.
- **Dreamcast:** the mixer's fixed `tone_pan` table became a table the main thread writes (`dcsnd_tone_pan`): left 192 / 64, right 64 / 192, middle the old table. Not built, not listened to.
- **Mix footer:** now "Sound, light, swing and pan play."
- **Not run:** the GBA / PS1 / DC builds, the GBA speed windows (the player does one compare per sounding note), the listening check on each console.

## ST-ADV: Lyric lines 9..16 (core + studio; host-tested, not built for GBA / PS1 / DC)
An Advanced song has 16 lyric lines. Lines 1..8 are the classic song's (`lyrics[]`, `line_singers[]`, the `'SONG'` chunk: untouched); lines 9..16
live in the `SongExt`.
- **Data (`song_ext.h`, `song_ext.c`):** `SX_XLINES` 8, `SX_LYRIC_LINES` 16, `SongExt.lyr[8][28]` and `lsing[8][4]` (little endian u32 as plain bytes, so the
  struct stays byte aligned), the last two members (the undo journal covers them: `STUDIO_JBYTES` is song + SongExt). `SxSection.line[k]` holds 0..15.
  Helpers: `songx_lines`, `songx_line_text`, `songx_line_buf`, `songx_line_singers`, `songx_set_line_singers`, `songx_lyr_any`. `songx_sanitize` clamps lines to
  0..15, fixes the words (printable, one NUL) and the singer bits, and `flags` may carry bit 5.
- **Saved page:** a lyric tail after the one-bar tail, only when `SX_FLAG_LYR` (16) is set (some line 9..16 has words or a singer mask): a u8 mask of the lines, then
  per set line its words (up to 27 bytes) + NUL and 4 mask bytes. A damaged tail drops lines 9..16 only. Pages without it encode as before. Worst case about 257 B
  more against `SONGX_PAGE_BYTES` (a page that does not fit still falls back to classic).
- **Player:** `song_line_mask_x(s, x, line)` (`song_line_mask` = the same with no SongExt); `begin_line` and the syllable vowel read the words through `songx_line_text`.
- **Studio:** `st_line_count / text / buf / mask / set_raw / touch_text / touch_mask` and `st_xmasks(XM_TOUCH / CLEAR / DROP / REMAP)` (`studio_adv.c`, with classic
  stubs when `STUDIO_ADV` is 0) are the only way studio code reads or writes a line; only the working song has more than 8. The LYRICS tab scrolls 16 lines over 8 rows
  (`studio.ltop`; two-digit numbers drawn at x + 8), every line tool (clear, fit, auto-fit all, auto parts, part cycler, choir fills, remove singer, sort by section)
  covers lines 9..16 and is one undo step. The Lyric lines view picks 1..16 (`e_line` packs `pass * 32 + choice`) and Fill cycles over 16. The stage and the Concert Hall
  karaoke read the words through the same helpers.
- **Tested (2026-11-16, host):** 7 new tests plus one stale test fixed. `test_song_ext.c`: lines 9..16 read / write / stay out of the classic song; the saved page (old page bytes unchanged with no words, a tail within the 257 B worst case, a page cut inside the tail drops lines 9..16 only); sanitize (no NUL, unprintable bytes, section lines clamp to 16); the player mask (limited to singers that exist, empty mask = first singer). `test_studio_tools.c`: `st_line_*` edits with undo / redo, a classic song has no lines 9..16, the LYRICS tab scrolls 16 lines over 8 rows. The Lyric lines view test still expected the wrap-around to end at line 8; it now ends at line 16. Easy keeps an Advanced song's SongExt, so its 16 lines stay in view after the switch back (words are never hidden).
- **Not done:** no GBA / PS1 / DC build (GBA scratch +256 B unmeasured), no look at the LYRICS tab on screen. (`studio_write_lyrics`, the "New words" tool, writes lines 9..16 since 2026-11-17: see below.)

## ST-ADV: New words write lines 9..16 (2026-11-17; core + studio, host-tested only)
`studio_write_lyrics` (SONG / LYRICS \"New words\", and Inspire on the working song) wrote lines 1..8 by the classic order and left 9..16 alone. On the working Advanced song (`st_line_count` 16) it now walks the 16 lines
and gives each the kind of the part that sings it first (`SxSection.line[pass % 4]`): Chorus gets the chorus line, Bridge the bridge line, Intro and Outro a hook with no words, any other part a verse (the hell
verses at 60% for a Hellvile muse, as before); a line no pass sings is emptied. Lines 9..16 are cleared first, so \"New words\" never leaves stale words behind. A classic song, and any song that is not the working one,
behaves exactly as before. The LYRICS tool touches lines 9..16 for the undo journal before it writes: one undo step takes all 16 back.
- **Tested:** `adv_new_words_write_lines_9_to_16_by_the_part_that_sings_them` (a verse line 11 gets words, an intro line 13 stays empty, a stale line 15 nobody sings is emptied); the full host suite passes (1660); `xcheck` gcc 438 files `-Werror`.
- **Not done:** no GBA / PS1 / DC build (the GBA, PS1 and Dreamcast cross compilers are not here: CI builds them), no look at it on screen.

## ST-ADV: what is left, and how to build it (written 2026-11-16, read from the code, nothing here is built)
One item is open (PS1 pages: tried, not affordable; pan playback, lyric lines 9..16 and patterns of 3 and 4 bars are built). Each has the files it touches and the reason it was not done with the others. Read this before starting one.

1. **Pan playback (built 2026-11-16, see "Pan playback" below).** Left / middle / right per lane, heard on the GBA, PS1 and Dreamcast ports (code written, not built here: see the section).
2. **More than 8 lyric lines (built and unit-tested on the host, see \"ST-ADV: Lyric lines 9..16\" below).**
3. **Patterns of 3 and 4 bars (built, see "ST-ADV: Patterns of 3 and 4 bars").** (old plan:) One-bar patterns are built ("One-bar patterns" above). Longer ones need cells of 64 or 96 or 128 steps, and a cell is 32 bytes in the pool and a fixed
   `[4][4][32]` in the classic song. A pattern would be a chain of 2-bar blocks (the same pool, one more map entry per block) played one after another; the arrangement and the Note grid
   would then address a block, not a pattern. Decide that chain model before writing code.
4. **PS1 pages (tried 2026-11-16, not affordable: see "ST-ADV: PS1 pages").** 63,760 B of RAM margin against about 22 KB of room above the gate; the PS1 stays classic.
Measured on CI (run on `5a08784`, 2026-11-17, all jobs green): core 1659 tests pass at 240x160 and 320x240 and under ASan / UBSan (the UBSan overflow reports are in `concert_gfx.c`, `fixmath.c`, `test_math.c`,
`test_concert_gfx.c` and a hash in `test_studio_tools.c:730`, none in song / studio ST-ADV code; they do not fail the run); `xcheck` 438 files `-Werror` on gcc, arm, mipsel and sh4; check-gen; the Dreamcast host tests;
the PS1 disc (ledger above); the GBA ROM and the headless harness. GBA Music Studio tabs (classic screens): song, notes, play, order, choir, PERFORM 30.0 fps; **LYRICS 29.9 fps (work 32.6 ms average, 34.4 ms max: the only
tab over the 30 ms budget; it scrolls 16 rows since lines 9..16, no run from before that change is in these logs to compare)**; PERFORM sung phase median 6.4 ms, worst 7.9 ms, 0 slow ticks; GBA `studio` .bss 6,368 B.
Still unmeasured: the Advanced screens themselves on the GBA (the speed script only opens the classic tabs: Note grid, Box, Mix, Expert, velocity strip, Lyric lines view, part view), GBA `.bss` before / after patterns of 3 and 4 bars
(+32 B computed), the Dreamcast and PS1 hardware or emulator runs, and a PS1 speed run of the studio.

## ST-ADV: Patterns of 3 and 4 bars (2026-11-16; core + studio, host-tested only)
Chosen model: a **continuation link**, not a block address. Pattern `p` plays its own cells (two bars, or one if short) and then the cells of
`cont[p]`, another pattern, in the same pass. The arrangement, the sections and the lyric passes still name the first block only. A chain is two
blocks, so a pass is 1, 2, 3 or 4 bars (16 / 32 / 48 / 64 steps). No cell, pool or classic-song change.
- **Data:** `SongExt.cont[32]` (0 = none, else next id + 1; +32 B). `songx_cont`, `songx_head_of`, `songx_set_cont` (refuses a link to itself, a chain of three,
  a block with two heads), `songx_pass_steps`, `songx_pass_bars`, `songx_free_pattern`, `songx_cont_any`. `songx_ticks` counts whole passes.
- **Saved page:** a tail after the one-bar mask, only when `SX_FLAG_CONT` (32) is set: a count, then (head, next) pairs (at most 65 B). A page without links
  is the old page; a damaged tail drops the links only. Sanitize drops links that break the rules.
- **Player:** `P.blk`. `advance()` goes into the second block instead of the next pass, `begin_line()` runs once per pass, the section fade fraction
  counts real pass lengths, `song_pos_pattern()` tells the block sounding. Solo / range loops are unchanged.
- **Note grid:** "Length: 1 to 4 bars" (cycles; 3 and 4 take the first free pattern as the block, one undo step) and "Other block" (jump between the two so each is
  edited like any pattern). Going back to 1 or 2 bars removes the link and keeps the block and its notes as an unplayed pattern. The length of a block is set on its head.
  The status line says "Pattern A: 4 bars, then F"; the playing cursor follows the block.
- **Tests:** 6 in `test_song_ext.c`, 3 in `test_studio_tools.c`; the one-bar test follows the new cycle.
- **Not done:** the part view's pass rows now say "one bar" / "3 bars, then F" / "4 bars, then F" (drawn, no test: the draw path has none); CI built the GBA, PS1 and host (see the measured lines in "what is left"); the Dreamcast host tests pass; the grid's speed window on the GBA is not in the speed script.

## ST-ADV: PS1 pages (2026-11-16: tried, measured on CI, NOT affordable; the PS1 stays at 0 pages)
`SONGX_BOOK_PAGES` 6 on the PS1 (patch `nov16m`) turned every ST-ADV screen on there. The PS1 CI ledger (job summary, `docs/PS1.md` \"RAM headroom\") then read **32,768 B left below the
stack top, 40,960 B under the 73,728 B gate**, and the job failed. The computed +13.7 KB in `nov16m` was wrong: it counted only `book_data` (9,216 B) and `Studio` (+4.5 KB) and
missed the code. Ledger lines with it on: `studio_adv` 17,164 B resident, `song_ext` 20,720 B, `book_data` 9,216 B, `studio` .bss 6,388 B, and the **overlay slot 153,600 B** (the studio group,
152,888 B, became the biggest group; it was 131,072 B). Measured after the revert, the pages cost **63,760 B** of margin in all (see below), against about 22 KB of room above the gate. The 132.6 KB margin quoted in `docs/PS1.md` is
older than the tip: always read the margin from the latest CI ledger, not from the doc.
- **Now:** the PS1 is back to 0 pages and the classic ORDER tab (the no-op `studio_adv.c` stubs). Everything else in ST-ADV (patterns of 3 / 4 bars, lyric lines 9..16, the part view's pass lengths)
  is unchanged and runs on the host, Dreamcast and GBA.
- **Measured after the revert (CI, run on `5a08784`, 2026-11-17):** 96,528 B left below the stack top (22,800 B above the 73,728 B gate, 1,776 B under the 96 KB aim), overlay slot 133,120 B, studio group
  131,488 B (the biggest group again; the planner's estimate was 132,584 B against story 130,012 B), `studio` .bss 1,868 B, no `book_data` / `studio_adv` in the resident list. With 6 pages the same ledger read
  32,768 B left, slot 153,600 B, studio group 152,888 B. So the pages cost 63,760 B of margin in total (before the revert I had estimated 53 KB from the ledger lines; the real number is higher because the studio group's growth in the slot and the resident code both count). Every PS1 job step is green (tool tests, tile-row host test, overlay planner tests, game disc, engine-test disc).
- **To do it later:** the studio group would have to stay under the 131,072 B slot (move ST-ADV code out of the group that sets the slot, or shrink the studio group by about 22 KB), the code
  (about 17 KB) would have to go into an overlay (`MODAL_CALLBACKS[\"studio_adv\"]` in `ps1/tools/ovl_plan.py`: `name_done key_done tempo_done fade_done newpart_done arrange_tools part_tools ly_tools box_tools
  grid_more grid_tools` all go to `st_popup` only), and the 9,216 B of pages would have to be fewer (2 pages = 3,072 B). Measure on CI after each step.

## ST-ADV: PS1 pages, second try (2026-11-17: tried, measured on CI, NOT affordable; the PS1 is back to 0 pages)
`SONGX_BOOK_PAGES` 2 on the PS1 plus `ovl_plan.py` `MODAL_CALLBACKS["studio_adv"]` (11 popup callbacks, all passed to `st_popup()` only). PS1 CI ledger: **37,920 B left below the stack top** (96,528 B before; gate 73,728 B,
35,808 B under it), the job failed. **Overlay slot 172,032 B** (133,120 B before), **studio group 170,176 B** (131,488 B before: +38,688 B, the ST-ADV code and its song_ext sections now sit in the studio group and make it the biggest;
`home` is 170,744 B but loads over `.borrow`, so it does not set the slot). Resident grew by 19,696 B besides the slot: `book_data` 3,072 B, `song_ext` 14,544 B (11,280 B text, 3,076 B bss).
- **What worked:** `studio_adv` is out of the 30 biggest resident objects (it was 17,164 B resident), so the callbacks entry did move it into the overlay.
- **What failed:** the slot. Moving code from resident to the group only shifts the cost into the slot while the studio group is the biggest. 2 pages instead of 6 saved little because the code, not the pages, is the cost.
- **Now:** back to 0 pages and the classic ORDER tab; the callbacks entry stays (harmless with the stubs).
- **Only way left:** ST-ADV in a group of its own (a scene edge from the studio, loaded when the ORDER tab opens) or about 37 KB cut from the studio group. Both need design first; do not retry the page count.

## ST-ADV: PS1 pages, third try (2026-11-17: DONE, measured on CI; the PS1 has ST-ADV with 2 pages, nothing cut)
Nothing is cut: ST-ADV (all of it) and 2 pages on the PS1 (`SONGX_BOOK_PAGES` 2, 3,072 B), like the GBA. The second try failed because the studio group (170,176 B with ST-ADV) became the biggest
and set the overlay slot (172,032 B, margin 37,920 B). The fix is the planner's own mechanism for big groups: `ps1/tools/ovl_plan.py` `BORROW_GROUPS = ("home", "creator", "studio")` links the studio group
over the 53,776 B of `.borrow` (world.c / view3d.c tables, the combat bot's search, the memory card's block buffer), the way the house and the creator already are.
- **Measured on CI (run on `52f08b6`, PS1 job green):** **78,880 B left below the stack top** (gate 73,728 B: 5,152 B above it; 96,528 B before ST-ADV pages, 37,920 B in the second try), **overlay slot 131,072 B**
  (133,120 B before, 172,032 B in the second try), studio group 170,176 B `loads at 0x801bf9c0 (over .borrow)`, story 129,912 B sets the slot again. The estimate was about 77 KB.
- **Where the margin went:** resident `song_ext` 14,544 B (11,280 B text, 3,076 B bss: `book_data` 3,072 B). The margin is 5 KB over the gate: the next resident addition to the PS1 has to pay for itself.
- **Why it is safe:** nothing of the studio draws a 3D view (`plat_view3d`: levels_select, combat), syncs the world, runs the combat bot or touches the card. Card I/O is `save_game()` from freeplay / story code, and
  `memcard.c` calls `ovl_borrow_take()` first. A load of a borrowing group bumps the generation, so world / view3d rebuild their tables when a 3D scene next runs.
- **Not measured:** the studio group's load time from the disc (about 170 KB, estimated 0.3 s), and the PS1 hardware or emulator runs of the Advanced screens (CI builds the disc and the ledger only).

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
| FILE: Studio level | Studio level: Easy / Advanced | toggles (A, LEFT, RIGHT) | the row, toast "Advanced: more tools soon" (older than the screens: the text still says soon) | press again | `studio_level_switch_is_kept_in_the_stats_and_the_save` |
| FILE: New song... | New song... | confirms if unsaved, blanks the song, opens the Start card | the card | journal | `studio_start_card_inspire_templates_and_skip...` |
| NOTES: cursor line | (under the grid) | describes the cell: bar, beat, track, note, interval, "off the scale" | text under the grid | n/a | `studio_cursor_cell_is_described_in_one_line_that_fits` (every cell, key and scale fits 240 and 320 px) |
| LYRICS: Fit this line (SELECT tools, was START) | Fit this line | auto-fits the selected line to its notes | toast "Line 2 fits now!" | journal | `studio_fit_one_line_moved_to_the_lyric_tools` |
| CHOIR: START | Play song | plays the song (it cycled a section before: A menu "Change section" has it) | the bar | stop with START | `studio_start_plays...` |
| Tab strip labels | Start / Edit / Arrange / Words / Choir / Show / File, L / R / numbers on the others | names the tabs | the strip | n/a | `studio_tab_help...` (labels set), runner shots |

Not in the ledger because they did not change: every SONG row, the NOTES keys, ORDER, the CHOIR grid, the PERFORM
show, the song book pages.

## PLAY on the GBA (ST-PLAY, measured 2026-11-15, libmgba, `make -C gba test-speed2 SPEED2="studio_play"`)
Base: the clean tip `d979c93`. `gba/harness/scripts/speed2_studio_play.txt` (STUDIO -107, the "Park United!" choir; the pads at
rest, a held pad, the loop, the count-in, a take with pads pressed, the drum pads). Every window: 30.0 drawn fps, 30.0 logic ticks/s.

| window | work avg ms | work max ms |
|---|---:|---:|
| pads at rest | 24.1 | 26.6 |
| a held pad | 23.5 | 26.4 |
| the loop (START) | 25.6 | 33.3 |
| count-in | 26.4 | 31.2 |
| recording (pads pressed) | 24.5 | 30.3 |
| drum pads (menu closed / loop) | 27.7 / 27.3 | 30.3 / 32.2 |

`make -C gba profile-studio`, before -> after: SONG idle 31.6 -> 32.0, NOTES idle 27.8 -> 27.7, ORDER 25.5 -> 26.3, LYRICS 32.4 -> 32.3, CHOIR 30.5 -> 30.0
(noise and the eighth tab pill); new: PLAY idle 23.6, PLAY looping 24.8. What it took: the pad notes (a scale shift is a few divisions each, ~4 ms
a frame when redone per frame) are computed when a setting changes, when the tab opens and eight times a second, never per frame; a pad is one
round rect and a shadow line. RAM: GBA `.bss` / IWRAM identical (the player's struct is repacked, the pad state is 64 B of the scene scratch,
taken from the bake block's pixel pool); PS1 resident 0 (scene scratch). The PS1 and Dreamcast were not measured: their toolchains were
not installed on the build machine.

## PLAY spikes (PLAY-31 / PLAY-32, CI run #857, libmgba)
`make -C gba profile-play` (CI step "Profile the Music Studio PLAY tab") runs `gba/harness/scripts/studio_play_idle.txt` and `studio_play_loop.txt` (330
ticks each) and prints the slowest ticks, the gaps between them, the loop's extra median / peak over idle, and the functions the slow frames spend extra
time in (`gba/build/play_*_spikes.log`). It reports and never gates. Result: idle 22.4 ms median / 25.4 worst, looping 23.4 / 29.8. The one spike in the
loop is audio and voice setup plus the pad-note refresh, not drawing, so pad drawing is not where to look. The header, Q line, count and chord labels are
built without `doomo_snprintf` (`play_q_text`, `play_head_text`, `play_chord_text`, `play_count_text`; the test compares them with the old formats).
Next steps and the open `profile-sung` question: `docs/design/held/status/STUDIO.md`, PLAY-32.

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
