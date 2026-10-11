# The Music Studio

*"Make a way to edit lyrics and music in the game."*

The Music Studio (`SCENE_STUDIO`) is where you write songs for the citizens of
Hellvile Park to sing: the tune on a 4-track tracker, the words line by line,
and a choir of up to **25 citizens** who sing it on stage. Every song uses one
shared format, `DoomoSong` (`core/include/doomo/song.h`), and one shared
player, so the Concert Hall's generated songs and your own are the same thing.

| File | What |
|---|---|
| `core/include/doomo/song.h` | the contract: `DoomoSong`, the player API, the song book |
| `core/src/song_player.c` | the player + helpers (timing, singing, choir, syllables, scales) |
| `core/src/song_book.c` | `doomo_song_book[6]` and the `'SONG'` save extension chunk |
| `core/src/studio.c` | the scene: tabs, input, undo journal, popups, citizen picker, unlock rules |
| `core/src/studio_edit.c` | SONG, NOTES (tracker) and ORDER tabs |
| `core/src/studio_words.c` | LYRICS and CHOIR tabs |
| `core/src/studio_stage.c` | PERFORM: the choir show |
| `core/src/studio_file.c` | FILE: song book, templates, Inspire me, undo |
| `core/src/studio_gen.c` | composer, lyric writer, 14 templates, choir fills, parts, auto-fit, word filter |
| `core/src/unlocks_studio.c` | unlock ids 240..255 and the activities |
| `core/tests/test_studio.c`, `core/tests/scripts/studio.txt` | tests and the runner tour |

You find it in the **Concert Hall** and the **Town Hall** (activities table
`doomo_activities_studio`, free play and story from chapter 1).

## Controls

GBA buttons only; PS1 extras in brackets.

| Anywhere | |
|---|---|
| L / R | previous / next tab (in NOTES: previous / next pattern) |
| SELECT + L / R | previous / next tab from any tab [L2 / R2] |
| SELECT (tap) | the tab's tools menu |
| SELECT + B | undo [Triangle] |
| START | play / stop |
| B | leave the studio (asks if the song is unsaved; in NOTES: erase / tie) |

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
| START | loop the pattern from the cursor |
| SELECT | copy / paste pattern or track, clear track, nudge a track a scale step |

Every placed note is previewed on its own channel (`song_preview`).

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
* START **auto-fits** the line: too few words get "la"s (or an "oh"), and
  if there is no room left the last notes merge into long ones (only when no
  other line uses the pattern); too many words split long notes into new
  onsets. SELECT: sing this line, auto-fit all, share out parts, write lyrics
  for me, clear the line.

### CHOIR
Up to 25 citizens on a 9/8/8 grid, each tile tinted by voice section with an
S/A/T/B badge. Sections are automatic from the voice pitch slider (>= 22
soprano, >= 15 alto, >= 8 tenor, else bass) and editable (START cycles
auto -> S -> A -> T -> B). A on a singer: hear their voice (they sing the first
sung line alone), change singer (the citizen picker), change section, remove.
SELECT **quick fills**: friends of the selected singer, a Hillvile choir, a
Hellvile choir, both districts, random 25, share out parts, sort by section,
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
the part label ("Altos (6)", "Full choir (25)"), and an applause meter.
Afterwards: stars, applause, an **ENCORE!!** at 80%+, and every singer gains
happiness (+6..+18) and social, and singers next to each other on the risers
grow a little closer. SELECT on the poster hands the song to the Concert Hall
stage when that module offers it (`concert_stage_song`).

### FILE
The song book (6 pages: save here / open / favourite / delete, with
confirmations), a blank song, **templates**, **Inspire me!**, undo (with the
number of steps left) and leave. Saving into the book is kept in memory and
written with the game save (the `'SONG'` chunk).

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

## Budgets
`.bss`: the book 6 x 892 B + stats 12 B = 5364 B; everything else 2002 B (the
working song, a 768-byte undo journal - run-length coded byte ranges, 16+
ordinary edits always fit and template loads are undoable too -, UI state,
the player). Const data ~11.6 KB (GBA thumb), code ~60 KB. No malloc, no
floats. The player's cost per tick is bounded: a few steps, <= 4 cached
`plat_tone` calls and at most one voice tuning search (~130
`audio_voice_freq` calls) per syllable.

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
