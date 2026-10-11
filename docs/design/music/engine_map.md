# DOOMOGACHI audio engine: technical map of the current system (GBA + PS1 + native)

Read-only snapshot of the repo at HEAD `43af439` (2026-10-11, ~16:40). All `file:line` refs are relative to the repo root. **Measured** numbers come from tools I ran in this folder only: GBA objects compiled into `music/build/`, the GBA `memreport.py` run on a copy of the 16:32 game-ROM map, the mGBA harness `profile` command run on copies of the 16:32-16:33 ROMs (`music/build/gba/`), and a host tool that runs the real sequencer (`music/build/songstat.c`). **Estimated** numbers are cycle counts from the generated ARM code or from ARM7TDMI / R3000A timing rules, and they are labelled. I did not edit, build in or run git in the repo.

Related specs: `../story/story_map.md` (appeared at 16:42; its audio section is consistent with this map, see section 13) and `../custom/wishlist.md` (audio options AU01-AU24, voice items A32-A36).

---------------------------------------------------------------------------------------------------
## 0. Fifteen facts to know first

1. **Three independent 4-channel sequencers drive the same 4 tone channels**, all at 30 Hz inside `doomo_frame()`:
   - the MML background-music sequencer: `core/src/audio.c` (19 songs in `core/src/songs.c`);
   - the Studio / shared `DoomoSong` player: `core/src/song_player.c`;
   - the Concert Hall's procedural composer + player: `core/src/concert_song.c`.
   The last two silence the first one with `audio_music(SONG_NONE)` and call `plat_tone()` themselves.
2. **The platform contract is tiny:** `plat_tone(ch 0..3, hz, vol 0..15, WAVE_*)` + `plat_sample(sfx, vol 0..15)` (`core/include/doomo/platform.h:122-130`). The core sends at most 4 tone states per 30 Hz tick. There is no notion of voices > 4, panning, effects, envelopes, samples-as-instruments or timing finer than a tick.
3. **The music clock is the logic tick.** `audio_tick()` runs once per `doomo_frame()` (`core/src/game.c:52`). On GBA, catch-up ticks run back-to-back when 3D frames take 50 ms (`gba/src/main.c:154-171`), so notes arrive in bursts. On PS1 the game slows down instead of skipping (`docs/PS1.md:317-322`), so the music slows down with it (pet 23 fps, exorcism 19-27 fps).
4. **Songs are short loops.** They loop every 10.5-32 s. The metal songs heard in combat loop every 10.5-12.8 s (section 2.7). No song has an intro, outro, variation, `L` loop point, transposition or mid-song tempo change.
5. **Music changes are hard cuts.** `audio_music()` restarts the sequencer at tick 0 (`core/src/audio.c:356-411`). There are no crossfades, no beat-synced switches and no layers. The overworld polls for the right song every 90 ticks (`core/src/overworld.c:646-649`).
6. **Bug: `ASONG_ONCE` is never read.** `audio_music(SONG_VICTORY)` loops the 6.4 s fanfare forever. That call is made at `combat_scene.c:294`, `exorcism_scene.c:557`, `levels_play.c:144`, `arena_modes.c:230` and `cutscene.c:101`. Only `audio_music_once()` (credits, minigames) plays it once (section 2.6).
7. **GBA music is pure PSG** (2 squares, wave, noise; `gba/src/audio.c:87-133`). DirectSound A carries only SFX, through a 4-voice C mixer at 10,512 Hz in the VBlank IRQ (`gba/src/irq.iwram.c`). DirectSound B, PSG sweep, PSG envelopes, the 64-sample wave mode, stereo panning, timer 1, DMA 0/2 and every IRQ except VBlank are unused.
8. **PS1 music uses 4 of 24 SPU voices** (looping single-cycle ADPCM waves). SFX use voices 4-23. Reverb, pitch modulation (FM), the noise generator, sweep volumes, ADSR, stereo panning, capture and the SPU IRQ are unused. SPU RAM is **35 KB used, ~469 KB free** (section 7.1).
9. **The platforms already sound different.** GBA plays `WAVE_SAW` as a 25 % pulse on channels 0/1 (`gba/src/audio.c:105-106`), and every DOOM lead and power chord is on channel 0/1. GBA plays pulse-wave "pitched drums" on channel 3 as 7-bit periodic noise. PS1 plays true saws and real pulse tones in both cases.
10. **GBA CPU is the binding constraint.** The heavy 3D scenes sit right at the 50 ms (20 fps) line: courtyard 19.4 fps, third person 19.2-20.3 (`docs/GBA.md:353-373`). The docs measured them with the SFX mixer idle. The current C mixer costs **~8.1k cycles per vblank for 1 voice (2.9 % CPU) and ~21.8k for 4 voices (7.8 %)** (measured, section 6.2). The title screen runs with ~0 % halt time at 30 fps (measured).
11. **GBA RAM is nearly full.** IWRAM has **316 B** spare beyond the 6 KB stack reserve (measured stack high-water: 1,744 B). EWRAM has **7,680 B free** (measured, map of 16:32). ROM is 2.43 MB of a possible 32 MB, so **~29.5 MB of ROM is free** for samples and wavetables.
12. **PS1 RAM:** 139 KB below the stack top, i.e. ~75 KB above the 64 KB stack reserve (`docs/PS1.md:237`). Audio is resident and costs **~108 KB of main RAM**. 48.3 KB of that is the 8-bit SFX PCM, which is *also* held as ADPCM in SPU RAM. Another 20 KB is the static staging buffers in `ps1/src/spu.c` (measured object sizes, section 7.4).
13. **Nothing renders audio to a file for tests.** The native platform only counts `plat_tone` / `plat_sample` calls (`core/native/plat_native.c:161-173`). There is no audio golden hash. The only WAV renderer is the trailer's float synth (`trailer/tk.c:536-584`), which is host-only. The GBA harness can measure mGBA's real output (peak-to-peak).
14. **Record scratches exist only in the trailer:** a noise sweep (`trailer/tk.c:508`) plus tape-stop slides (`trailer/music.c:166-168, 532-534, 685-687`). The game has no scratch SFX. It does draw spinning records in the Studio song card and the Jukebox.
15. **Rhythm-coupled scenes need music time = logic time.** The Exorcism rhythm game is tick-locked to `EXORCISM_BPM 120` and `SONG_EXORCISM` at t120 (`core/include/doomo/exorcism.h:32`). The Concert and Studio stages read `song_beat_age()` and `PEV_*` events for gameplay and visuals (`core/src/concert_scene.c:526, 575`).

---------------------------------------------------------------------------------------------------
## 1. Data flow

```
 scene code ──audio_music(SONG_x) / audio_music_once()──┐
            ──audio_sfx(SFX_x)─────────────┐            │
 ui_dialog / creator / players            │            ▼
            ──audio_voice_char(voice, c)──┐│   core/src/audio.c  (30 Hz, in doomo_frame, game.c:52)
                                          ││     MML parser x4 ch  -> envelope/vib/arp/sweep -> send()
 song_player.c (DoomoSong) ──plat_tone────┼┼──┐  voice babble on ch1 (wins while a blip sounds)
 concert_song.c (ConcertPlayer) ─plat_tone┼┼──┤  SFX: opt volume -> plat_sample()
                                          ▼▼  ▼
                 plat_tone(ch0..3, hz, vol0-15, WAVE_*)        plat_sample(sfx, vol0-15)
        ┌────────────────────┬───────────────────────┬──────────────────────┐
        GBA gba/src/audio.c   PS1 ps1/src/spu.c        native plat_native.c
        PSG sq1 sq2 wave noise SPU voices 0-3 (ADPCM   counters + optional log
        + DS-A mixer (IRQ)     wave loops) + 4-23 SFX  (no audio rendered)
```

---------------------------------------------------------------------------------------------------
## 2. Core MML sequencer (`core/src/audio.c`, `core/src/audio_int.h`, `core/src/songs.c`)

### 2.1 Tick rate and timing math

* Runs once per logic tick (30 Hz): `audio_tick()` (`audio.c:435-456`) is called from `doomo_frame()` before `scene_update()` (`game.c:48-56`). So an `audio_music()` call made in a scene update takes effect on the next tick (1 tick of latency).
* Durations are in units of 1/192 of a whole note (`AUDIO_WHOLE 192`, `audio_int.h:31`). A quarter note is 48 units.
* Tempo: `rate256 = (bpm*12288 + 900)/1800` = units*256 per tick (`audio.c:159-164`). bpm is clamped to 30..300. Example: at 120 bpm there are 3.2 units per tick, so a 16th (12 units) = 3.75 ticks. At 184 bpm a 16th = 2.45 ticks (uneven 2/3-tick 16ths).
* Each channel has `wait` (1/256 units). Every tick it subtracts `rate256` and parses until `wait > 0` again. The parse is bounded by `guard = 256` bytes per channel per tick (`audio.c:208-293`). Timing errors do not accumulate, and channels stay in lockstep.
* Envelope, vibrato, arpeggio and sweep are evaluated once per tick (`chan_shape`, `audio.c:296-353`). So **arpeggios step at 30 Hz** (a 4-step table cycles at 7.5 Hz, `arp_tab[..][t & 3]`, `audio.c:330`), and vibrato is sampled at 30 Hz. Classic chip drivers update at 50/60 Hz.

### 2.2 Song data format: an MML dialect, one const string per channel (`audio_int.h:1-24`, numbered in-file)

`t<bpm>` (global; read from channel 0 at song start, `audio.c:385-390`) · `o<n> < >` octave · `l<n>[.]` default length · `c d e f g a b` with `+ # -`, length, dots, `^` ties · `r` rest · drums `K S H O C T P` (kick, snare, closed hat, open hat, crash, tom, clap) · `v<n>` channel volume 0..15 · `@<n>` instrument · `q<n>` gate 1..8 · `k<n>`/`k-<n>` transpose · `[ ... ]<n>` repeat (nesting up to 4) · `L` loop point · spaces, `|` and newlines are ignored.

```c
typedef struct AudioSong { const char *name; uint8_t flags; const char *ch[AUDIO_CH]; } AudioSong;  // audio_int.h:35-39
```

Validation: `measure()` expands loops and sums the units per channel. `audio_song_check()` requires every channel to parse and all channels to have the same length (`audio.c:461-537`).

### 2.3 Channels

| core ch | role in songs | notes |
|---|---|---|
| 0 | lead | carries the tempo |
| 1 | harmony / chords | **also the voice babble channel** (`AUDIO_VOICE_CHANNEL 1`, `audio.h:101`): the music is replaced while a blip sounds (`audio.c:351`) |
| 2 | bass | GBA: the wave channel |
| 3 | drums | swept noise "hits" (`drums[]`, `audio.c:46-57`); GBA: the noise channel |

### 2.4 Instruments and waveforms

* Waveforms (`platform.h:125-126`): `WAVE_PULSE12, PULSE25, PULSE50, TRIANGLE, SAW, NOISE`. That is all a song can ask for.
* 24 instruments, 12 bytes each (288 B) (`songs.c:25-50`). `AudioInst` = wave, attack, decay, sustain, release, vib depth/speed/delay, arp, sweep, gate, level (`audio_int.h:46-59`). They include: cute lead, soft harmony, triangle bass, bell, major/minor/dim/sus/maj7 chord arps, metal lead, power chord, saw bass, pad, spooky wobble, organ, music box, zap, LCD beep, flute, octave arp, trill, slap bass, chug, brass.
* Arp tables: none, major, minor, octave, power, sus4, dim, maj7, trill. These are 4-step tables (`audio.c:39-43`).
* Drums: 7 fixed noise sweeps `{f0, f1, vol, decay, sweep ticks}` (`audio.c:46-57`). The clap retriggers at t == 1.
* Pitch: `pitch_hz()` interpolates a 13-entry C4..C5 Hz*64 table at 1/16-semitone resolution and clamps to 20..16,000 Hz (`audio.c:14-37`).

### 2.5 Volume chain

`level (0..240 envelope) x channel v (0..15) x OPT_MUSIC_VOL (0..10, default 7) / 2400 -> 0..15` (`audio.c:344-347`). At the default option the peak is ~10.5 of 15. Updates go through `send()`, which only calls `plat_tone` when something audible changed (`audio.c:101-112`).
**Inconsistency:** `song_player.c` and `concert_song.c` scale by `vol*mv/7` (`song_player.c:506`, `concert_song.c:450-455`), so at default 7 they are at unity and louder than the BGM sequencer. Options are 0..10 (`core/src/options.c:64-67`: music 7, sfx 8, voice 8 default).

### 2.6 Looping, stingers, transitions, tempo and key

* **Loop:** when a channel's string ends, it jumps to `L` or to the start (`audio.c:214-220`). Because all channels have equal length, they wrap together. No song uses `L` (checked with grep), so **every song loops from bar 1 with no intro**.
* **Stinger:** `audio_music_once(song)` remembers `prev_song`, plays the song with `once_mode`, and restarts `prev_song` **from tick 0** when every channel has ended (`audio.c:413-420, 451-455`).
* **`ASONG_ONCE` flag is dead data:** `AudioSong.flags` is never read by `audio.c`. Only `once_mode` stops a song. `SONG_VICTORY` (flagged ONCE) started with `audio_music()` loops forever (see fact 6).
* **Transitions:** none. `start_song()` does `memset(ch,0)` and restarts (`audio.c:356-391`). The previous notes keep sounding until the next tick's `send()`. No crossfade, fade-out, beat quantisation or shared tempo.
* **Tempo change:** `t` works mid-song and is global (`audio.c:260`), but no song uses it. **Key change:** per-channel `k` transpose (`audio.c:268-272`) is unused. `DoomoSong` has one key and bpm per song (the Studio's key change rewrites the notes). The Concert encore is +10 % bpm (`concert_song.c:974`).
* **Dynamic music today** is only a hard switch on a threshold. Example: Hell Breach levels switch base song → hell song when the hell meter reaches `hell_at`, and → `SONG_BOSS` while a boss lives (`core/src/levels.c:540-555`). The overworld switches by map and story beat (`overworld.c:138-150`).

### 2.7 The 19 songs: size, length, use (measured with `music/build/songstat.c`, which runs the real `audio.c` + `songs.c`)

| id | name | bpm | MML bytes (ch0/1/2/3) | loop | notes/pass | `plat_tone` calls/tick | plays at (file:line) |
|---|---|---|---|---|---|---|---|
| 1 TITLE | Doomogachi Life! | 140 | 465 (156/142/116/51) | 13.7 s | 202 | 2.91 | title `fp_title.c:57`; ch1 card `cutscene.c:93` |
| 2 HAHALAND_DAY | HaHaland Day | 124 | 768 | 31.0 s | 300 | 2.48 | overworld day `overworld.c:148`; free-play island `fp_common.c:47-49`; `CUT_ARRIVAL` `cutscene.c:97`; fishing `fish_scene.c:242`; concert restore `concert_song.c:989`; maps hahaland / sugar_rush_sewers |
| 3 HAHALAND_NIGHT | HaHaland Night | 80 | 342 | 24.0 s | 169 | 1.36 | night variants of the above; dreams `dream.c:277,310`; Library lobby `lib_common.c:662` |
| 4 CREATOR | Make a Friend! | 132 | 675 | 14.6 s | 192 | 3.47 | `creator.c:1727`, `fp_newisland.c:40` |
| 5 SHOP | Shop Hop | 116 | 434 | 16.6 s | 162 | 2.64 | `fp_shop.c:133`, cooking `cook_scene.c:417` |
| 6 CHURCH | St. Cuddles Hymn | 72 | 287 (no drums) | 26.7 s | 69 | 0.53 | exorcism church `exorcism_scene.c:208`; map bone_cathedral |
| 7 PET | Doomogachi Beeps | 150 | 418 | 12.8 s | 174 | 1.65 | `pet_scene.c:175`, `CUT_HOME` |
| 8 HELLVILE | Hellvile After Dark | 108 | 533 | 17.8 s | 144 | 2.71 | overworld `overworld.c:143`, ch4 card, Hellvile apartments `fp_apartments.c:80`, black market, level 5 hell song `levels.c:38` |
| 9 EXORCISM | The Ritual | 120 | 408 | 16.0 s | 161 | 1.93 | rhythm game `exorcism_scene.c:253` (tick-locked to `EXORCISM_BPM`) |
| 10 WORKSHOP | Rivet's Workshop | 110 | 624 | 17.5 s | 187 | 2.58 | `workshop.c:194,213`, `weld_game.c:105`, map plushie_factory |
| 11 INVASION | Hell Invades | 168 | 1021 | **11.4 s** | 266 | 2.96 | invaded overworld `overworld.c:142`, ch3 card, eruption `cutscene.c:113`, arena `arena.c:115`, horde `arena_modes.c:166`, levels 4/7 hell song |
| 12 HELL_BREACH | Hell Breach | 150 | 681 | **12.8 s** | 205 | 2.90 | level select `levels_select.c:73,123`, levels 1/3/6 hell song, minigame hell mode `mg.c:212` |
| 13 BOSS | Lord Vapula | 184 | 809 | **10.5 s** | 325 | 2.99 | boss phase `levels.c:550`, boss rush `arena_modes.c:166`, cloud_nine_spire |
| 14 VICTORY | Victory! (ONCE) | 150 | 255 | 6.4 s | 57 | 2.41 | see fact 6; `audio_music_once` in `credits.c:84,89`, `mg.c:325` |
| 15 SAD | Goodbye, Friend | 66 | 299 (no drums) | 29.1 s | 97 | 1.32 | defeats `combat_scene.c:295`, `arena.c:171`, `exorcism_scene.c:569,578`, `levels_play.c:150`; graveyard `fp_graveyard.c:31`; farewell `fp_resident.c:363` |
| 16 FESTIVAL | Hellvile Park Festival | 144 | 996 | 26.7 s | 378 | 2.75 | park united `overworld.c:144`, `CUT_WALL_FALLS`, minigames `mg.c:236`, `mg_wheel.c:131`, mg_catch/derby/gallery/memory/whack tables |
| 17 CREDITS | Everyone Together | 120 | 692 | 32.0 s | 342 | 2.76 | `credits.c:67`, `CUT_FREEPLAY` |
| 18 VAPE_LOUNGE | Cloud Nine Lounge | 88 | 399 | 21.8 s | 130 | 2.11 | ch2 card `cutscene.c:94`, `mg_dodge.c:444` |
| 19 OMINOUS | The Wall | 70 | 192 | 27.4 s | 51 | 1.14 | story beats C1_WALL, C2_SCREAM..EXORCISE `overworld.c:146-147`; `CUT_FOG`/`CUT_ERUPTION`; nightmare `dream.c:250`; map graveyard_shift |

Totals: **10,298 B of MML** + 288 B of instruments. `songs.o` is 11,510 B of THUMB const data (measured; it is all in ROM on GBA and resident in RAM on PS1). The Jukebox plays any of them (`core/src/lib_jukebox.c:138-162`). Story chapters unlock them (`core/src/story.c:89-95`).

### 2.8 Cost (measured on GBA, game ROM, title song)

`audio_tick` body (chan_advance/chan_shape inlined, THUMB in ROM) is **≥ 1.9k cycles per tick**. That is a lower bound: the profiler lists only the 400 hottest addresses. `plat_tone` adds **~80 cycles per tick** (harness `profile 3000000 audio_tick` / `plat_tone`, `music/build/gba/prof_title2.out`). Including `pitch_hz`, `send` and the divides, the total is ≈ 2.5-3k cycles per tick, **≈ 0.5 % of a 30 Hz tick** (561,792 cycles). `audio.o` is 7,245 B THUMB text + 374 B bss.

---------------------------------------------------------------------------------------------------
## 3. Sound effects

* 35 `SFX_*` (`audio.h:20-26`). The PCM comes from `tools/sfxgen.py`, which uses integer math only so its output is byte-identical everywhere (`make -C core check-gen`, `core/Makefile:135-142`). It is written to `core/gen/sfx_data.c`: **47,793 B of signed 8-bit mono, against a budget of 49,152 (`AUDIO_SFX_BUDGET`, `audio.h:86`), so 1,359 B are left**. The rates are 10,512 Hz (the GBA mixer rate, "no resampling"), 8,000 Hz or 6,000 Hz (`tools/sfxgen.py:31-35`). Total length is 7.4 s. The longest are EVOLVE 472 ms and POSSESS/BELL 433 ms.
* `audio_sfx(sfx)`: `OPT_SFX_VOL` 0 mutes, else vol = opt*15/10 → `plat_sample` (`audio.c:589-597`).
* **Priority and ducking: none.** GBA steals the oldest of 4 voices (`gba/src/audio.c:147-151`). PS1 uses an ended voice (ENDX) or else the oldest of 20 (`ps1/src/spu.c:286-293`). Music is never ducked under SFX or dialogue. On PS1, SFX peak amplitude is 0x3800 against 0x2000 for music tones (`spu.c:194, 297`).
* Most used: CURSOR 171 call sites, SELECT 101, CANCEL 75, ERROR 73, BLIP 58, SPARKLE 38 (grep count over `core/src`).

## 4. Voice babble (`audio.c:600-692`)

* `audio_voice_char(voice[5], c)` is called per typed character from dialogue (`core/src/ui_dialog.c:170-177`), the creator preview (`creator.c:697`), and per sung syllable from `song_player.c:645` and `concert_song.c:832`.
* Vowels and the first letter after a pause start a 1-3 tick blip on **tone channel 1**. Consonants only nudge the pitch. `!`/`?` bend it. Sliders: PITCH (A2..~A5), SPEED (blip length), QUALITY (12.5 % / 25 % / 50 % pulse or triangle), INTONATION, ACCENT (`audio.c:617-677`). It has its own xorshift (`vb.rng`), so it never touches game RNG.
* While a blip sounds it owns channel 1. The music's harmony is cut and comes back by itself (`voice_tick`, `audio.c:679-692`; `song_player.c:523, 795`; `concert_song.c:872, 925`).
* The singing players "tune" a voice to a note by searching the PITCH/ACCENT sliders through `audio_voice_freq()`: ~130 calls per syllable (`song_player.c:562-594`).

---------------------------------------------------------------------------------------------------
## 5. The Studio format and players (`song.h`, `song_player.c`, `concert_song.c`)

### 5.1 `DoomoSong` (`core/include/doomo/song.h:42-67`, 892 B)

| field | value |
|---|---|
| tracks | 4 (0 lead = sung melody, 1 harmony, 2 bass, 3 drums), `SONG_TRACKS` |
| steps | 32 per pattern (2 bars of 16ths), 4 patterns, order list ≤ 16 (`song.h:17-20`) |
| cells | melodic 0 rest, 1 hold, 2..127 MIDI; drums `SONG_DRUM_*` (kick, snare, hat, open hat, clap, tom, crash, cowbell) |
| per song | title[16], flags (VALID/CUSTOM/LOOP/FAVORITE/HELLISH), genre (16 `MUSIC_*`), bpm 60..220, key 0..11, scale (major, minor, pentatonic, blues, harmonic, chromatic), swing 0..8, `wave[4]` (track 3 = drum kit, `SONG_KIT_*`: Toy Box, Bone Rattle, Boom 808, Plush Pillow, Hell Forge, Classic), `vol[4]` |
| lyrics | 8 lines x 28; line i is sung over order position i (8..15 repeat) |
| choir | `singer[25]` resident ids, `line_singers[8]` bitmasks, `choir_layout`, `section[13]` nibbles (S/A/T/B), seed |
| storage | `doomo_song_book[6]` = 5,352 B **EWRAM** (`song_book.o` bss 5,364 incl. stats); saved RLE-packed in the `'SONG'` save-ext chunk, ~5.3 KB for a full book against `DOOMO_SAVE_EXT_MAX` 8 KB (`core/include/doomo/save.h:49`, `docs/STUDIO.md` "Persistence"). The struct is **append-only** (`song.h:62`). |

### 5.2 `song_player.c` (shared by Studio and Concert)

* Timing: 16 * bpm units per tick, 7,200 units per straight 16th (450/bpm ticks); swing moves 450*w units (`song_player.c:551-555, 896-923`). It does at most 4 steps per tick.
* ch0 lead + vibrato on held sung notes. ch1 harmony, or choir doubling a third / fifth / octave by section, with a chorus wobble that grows with singer count (`choir_double`, `649-669`; `render_tones`, `775-812`). ch2 bass. ch3 drum kits: 6 kits x 8 swept noise / pulse blips (`kits[]`, `422-459`). The envelope is a simple linear droop (`env`, `768-773`).
* Pauses the BGM on start (`start`, `827-860`) and restores it on stop (`song_stop`, `877-892`). Editor previews: `song_preview` (`925-949`).
* Measured size (GBA THUMB): 9,477 B text + 120 B bss. The Studio composer `studio_gen.c` is 16.5 KB. Templates are ~70 B each (`docs/STUDIO.md`).

### 5.3 Concert Hall (`concert_song.c`, `concert_int.h:92-190`, `concert_scene.c`)

* Its own format, `ConcertSong` (singer, genre, take → key, bpm, 2 chord progressions, 2 x 64-step melodies, 12 lyric lines). It also has its own player, `ConcertPlayer`, with a 20-bar form: intro 2, verse, chorus, verse, chorus, outro 2. The encore is chorus + outro at +10 % bpm. 16 genres (`concert_genres[]`, `concert_song.c:47`) give scale, waves, chord style (arp/pad/offbeat/alberti/stab/power), bass style and one of 13 drum masks (`drum_pats`, `23-41`).
* Drums are hard-coded tone sweeps on ch3 (`drum_tick`, `729-758`). The kick is `WAVE_PULSE50` at 140 → 50 Hz. On GBA that becomes periodic noise (fact 9).
* It returns `PEV_*` events (step, beat, bar, onset, kick, snare, section, end) that drive the stage show (`concert_play_tick`, `890-964`).
* Concert scene: `audio_music(SONG_NONE)` on enter (`concert_scene.c:282`), restore on leave (`327`). Studio songs staged in the hall run through `song_tick()` (`574`), concert songs through `concert_play_tick()` (`579`). Start: `683-695`. `concert_song_export()` converts to `DoomoSong` (`concert_song.c:1021`).
* Measured size: 12,274 B THUMB text.

---------------------------------------------------------------------------------------------------
## 6. GBA backend

### 6.1 PSG = the music (`gba/src/audio.c:22-133`)

| core ch | GBA channel | mapping | quirks |
|---|---|---|---|
| 0 | square 1 (`SND1`) | duty 12.5 / 25 / 50; **SAW → 25 %**; TRI → 50 % (`105-106`) | sweep register forced off (`110`); envelope step 0 (fixed volume). **Any volume or wave change restarts the channel** (`97, 111`), resetting phase every tick during decays (30 Hz zipper) |
| 1 | square 2 (`SND2`) | same | voice babble channel |
| 2 | wave (`SND3`) | 32 x 4-bit tables: 12.5/25/50 % pulse, triangle, saw, "noise-ish" (`28-42`); bank double-buffer swap on wave change (`44-56`) | only **4 volume steps** (100/75/50/25 %, `122`); 64-sample mode unused |
| 3 | noise (`SND4`) | `WAVE_NOISE` → 15-bit LFSR, any other wave → **7-bit periodic** (`128`) | always restarted |

Frequency: squares `2048 - 131072/hz`, which gives coarse resolution at high notes (~26 cents at 2 kHz). Wave `2048 - 65536/hz`. Noise divisor search (`58-85`). Setup in `audio_hw_init` (`169-181`): SOUNDCNT_L 0xFF77 (PSG master 7, all 4 channels L+R), SOUNDCNT_H = PSG 100 %, DS A 100 % on L+R, timer 0, **DS B off**. SOUNDBIAS 0x200 (9-bit at 32.768 kHz PWM).

### 6.2 DirectSound A = SFX only (`gba/src/irq.iwram.c`, `gba/src/audio_int.h:8-19`)

* `MIX_RATE 10512` (timer 0 reload 65536-1596, `irq.iwram.c:91`) = **exactly 176 samples per vblank**. DMA1 is in FIFO repeat mode from `mixbuf[2*176]` (IWRAM), restarted every other vblank (`62-75`). The mixer runs **inside the VBlank IRQ** (`gba_irq_dispatch`, `77-83`; the only enabled IRQ, `105`). There are `MIX_VOICES 4` of `MixVoice` (pcm, 20.12 pos/step/len, vol 0..15), nearest-neighbour resampling, an `int16 acc[176]` and an 8-bit clip pass (`21-60`). Voices are started from `plat_sample` with IRQs masked (`gba/src/audio.c:136-166`).
* IWRAM footprint: 484 B code (`.text.gba_irq_dispatch`) + `mixbuf` 352 + `acc` 352 + `mix_voice` 96 (measured object). `mixer_init` stays in ROM (`ROM_COLD`).
* **Cost, measured** (harness `profile 400000 gba_irq_dispatch` on the engine-test ROM while its DS samples play, `music/build/gba/prof_et.txt`): the IRQ took **4.7 % of all cycles** with ~1.75 voices active. Per-address cycles give **~26 cycles per output sample for a voice loop** (the `ldrsb` from ROM alone is ~6), and **~20 cycles per sample for the clip pass**. Model, consistent with the measurement:

| active SFX voices | cycles / vblank | % CPU | added to a 20 fps (3-vblank) 3D frame |
|---|---|---|---|
| 0 | ~0.3k (memset) | 0.1 % | ~0 |
| 1 | ~8.1k | 2.9 % | 1.45 ms |
| 2 | ~12.7k | 4.5 % | 2.27 ms |
| 3 | ~17.2k | 6.1 % | 3.08 ms |
| 4 | ~21.8k | 7.8 % | 3.90 ms |

  The engine-test fps table in `docs/GBA.md:353-366` was measured with the mixer idle: the ET plays samples only on ticks 1-8 (`gba/harness/scripts/enginetest.txt:4-11`). Real combat with gunfire is therefore already slower than the docs show. The C mixer is ~3x slower than a hand-written ARM mixer, and that is the cheapest CPU to win back (section 11.1).

### 6.3 Timing and pacing interplay

* 30 Hz logic. A drawn frame every 2 vblanks at best. If a frame overruns, the next tick runs with drawing skipped, but never twice in a row, so the floor is 15 fps (`gba/src/main.c:154-171`, `docs/GBA.md:259`).
* At 20 fps (50 ms frames), logic ticks come in pairs (a drawn tick, then an immediate catch-up tick). The music's tick-quantised events (note-ons, arps, drum sweeps) therefore jitter by up to ~50 ms in 3D. That is audible on 16th-note double kicks at 150-184 bpm.
* The VBlank IRQ is the only steady clock: 59.73 Hz, 280,896 cycles. Timer 1, DMA0 and DMA2 are free. Timers 2+3 are the profiling cycle counter (`main.c:216-219`). DMA3 does copies and fills.
* Flash saves run with IRQs off while the chip is in command mode (`docs/GBA.md` §8), so DS output stalls during saves. The PSG keeps its last tones.

### 6.4 GBA budgets (measured 16:32 map unless noted)

| resource | used / total | free | notes |
|---|---|---|---|
| CPU, 3D scenes | courtyard 43.9 ms r3d → 19.4 fps; third person 19.2-20.3; stairs / corridor 19.9 (`docs/GBA.md:355-366`) | **≈ 0 ms** before frames slip to 15 fps | 1 % CPU = 2,809 cycles per vblank = +0.5 ms per 50 ms frame |
| CPU, UI scenes | title 30 fps with ~0 halt (measured profile: `vblank_intr_wait` not in the top 25); keyboard 22.3 fps, dialogue typing 24.0 fps (`docs/GBA.md:404-405`) | up to the 50 ms line (requirement: UI ≥ 20 fps); typing / keyboard already near it | the babble plays exactly in the dialogue-typing case |
| IWRAM | 25,540 / 32,768; 6,460 left for the stack | **316 B** beyond the 6 KB `ASSERT` | measured stack high-water 1,744 B (`docs/GBA.md:170`); a 4 KB reserve would free ~2 KB |
| EWRAM | 254,464 / 262,144 | **7,680 B** | big items: town 109.5 KB, map 32.8 KB, portrait pool 18.4 KB, `gba_big` 17 KB (renderer / overworld / save, busy in 3D), scene scratch 12 KB, song book 5.35 KB. Candidates named in the docs: combat BFS 4 KB, dialogue queue 3 KB (`docs/GBA.md:172-181`) |
| ROM | 2,428,644 B (file padded to 4 MB) | **~29.5 MB** to the 32 MB max | WS0 3/1 + prefetch: random byte reads ~6 cycles, sequential `ldm` ~2 per word |

Unused GBA sound hardware: DS B FIFO + DMA2 (stereo, or a second stream at its own rate on timer 1), PSG sweep (ch1), PSG volume envelopes, PSG L/R routing per channel (SOUNDCNT_L bits 8-15), wave 64-sample mode (bank pair), length counters, SOUNDBIAS resolution modes.

---------------------------------------------------------------------------------------------------
## 7. PS1 backend (`ps1/src/spu.c`, `ps1/src/adpcm.c`)

### 7.1 SPU layout and use

```
SPU RAM (spu.c:14-18)                           measured / computed
0x00000-0x00FFF  capture buffers (hardware)     CD L/R + voice 1 + voice 3, 1 KB each (512 x 16-bit)
0x01000-0x01FFF  silent block + waves           1,840 B used: 16 silent + 5 waves x (112-cyc 128 B + 28-cyc 32 B) + noise loop 1,024 B
0x02000-0x7DFFF  SFX, bump allocated            27,616 B ADPCM for all 35 SFX -> top 0x08BE0
                                                **469 KB free** (0x08BE0..0x7E000)
0x7E000-0x7FFFF  "reverb work area (unused)"    8 KB: too small for most presets (section 11.2)
```

* Tones: core ch n → **SPU voice n (0-3)**. Waves are built at boot by synthesis + the in-house ADPCM encoder: two cycles per wave, looping from the second so the predictor history is right (`build_waves`, `110-153`). A long 112-sample and a short 28-sample cycle per wave keep the pitch register in range. Pitch = `hz*cycle*4096/44100`, clamped at 0x3FFF (`158-163`); the long cycle is used below ~1.57 kHz and tones top out at 6.3 kHz. Noise is a 1,792-sample random loop played at `hz*4` (`185-188`).
* `plat_tone` (`165-209`): writes L = R volume (`vol*15*0x2000/225`, **mono centre**), writes pitch every call, and re-keys only when the wave or cycle variant changes (gliding without clicks). ADSR is fixed (instant attack, short release). vol 0 → key-off. **Volume changes are written directly**, so they are smooth (unlike GBA).
* Key-on and key-off are written **one voice at a time** (`SpuSetKey(1, 1u<<ch)`, `202, 273`) instead of one batched KON write per tick.
* SFX: voices **4-23** (20). The core's 8-bit PCM is converted to ADPCM with the "fast" encoder on first use, or one effect per frame in the background from frame 2 (`audio_hw_frame`, `324-331`; `load_pcm8`, `214-240`). Sample rate comes from the pitch register.
* `audio_set_volumes()` (`301-305`) is never called (dead).
* Encoder quality (host test `ps1/harness/adpcm_test.c`): pulse 22-23 dB SNR, triangle 44.7, saw 17.8, 8-bit chirp 24.9 (`docs/PS1.md` §8). Fast-mode cost (estimated from `adpcm.c:61-115`): 5 filters x (28 + 3 x 28) inner iterations per 28 samples ≈ 20 multiply-heavy iterations per sample, **~500-800 R3000 cycles per sample**. That is fine for a one-off SFX conversion and too slow for real-time streaming.

### 7.2 Timing

* 30 Hz logic: `VSync(2)` (PAL 2-2-1) in `gfx_end` (`ps1/src/gfx.c:405-411`). `audio_hw_frame()` runs from `plat_end_frame` (`ps1/src/main.c:26-30`). **No frame skipping:** slow scenes slow the music (pet 23 fps, exorcism 19-27, combat 25; `docs/PS1.md:550-553`).
* Scene switches between overlay groups do a **blocking CD read** (`CdReadSync`, `ps1/src/overlay.c:60-90`): ~0.3 s per 70 KB, and three reads when one overlaid scene switches to another. No `audio_tick` runs during that time, so **the 4 tone voices drone on their last note** until the core resumes. No VSync callback or root-counter IRQ is installed. Root counter 1 is the hblank profiling clock (`ps1/src/timer.c`); counters 0 and 2 are free.

### 7.3 Hardware unused today

Reverb (EON per voice, work area, 32 config registers; PSn00bSDK only names `SPU_REVERB_VOL_*`, `SPU_REVERB_ON*` and `SPU_REVERB_ADDR`, so the 0x1F801DC0-0x1DFF block must be defined by us); pitch modulation (`SPU_FM_MODE1/2`, voice n by voice n-1); the noise generator (`SPU_NOISE_MODE1/2`, one global noise clock); sweep-mode volumes (hardware fades); ADSR envelopes; per-voice L/R pan; capture-buffer reads (`SpuRead`); the SPU IRQ address; CD-DA/XA input (the CD is also busy loading overlays).

### 7.4 PS1 budgets

| resource | value |
|---|---|
| CPU | 33.87 MHz, 1,128,960 cycles per 30 Hz frame. First person CPU 11-21 ms; pet 43 ms; exorcism 33-58 ms (`docs/PS1.md:536-553`, PCSX-Redux, documented as 1.5-2x slower than hardware). The SPU mixes in hardware, so per-voice register writes are the only per-tick audio cost. |
| main RAM | image ends `0x801dd310`, **139 KB below the stack top, ~75 KB above the 64 KB stack reserve** (`docs/PS1.md:237`). Overlay slot 111,376 B (set by combat); studio group 44 KB and concert group 51 KB leave ~60 KB of slot unused while those scenes run (`../story/story_map.md` §1.3) |
| resident audio RAM (measured objects, mipsel; core `-Os`, platform `-O2`) | `sfx_data` 48,280 · `song_player` 13,256 + 120 bss · `songs` 11,560 · `audio` 9,172 + 374 bss · `spu` 4,928 text + **20,076 bss** (static staging: `tmp[1024]` u32, two `pcm[1792]` + `adpcm[1024]` pairs, selftest `buf[1024]` + `chirp[2205]`) · `adpcm` 1,480 ≈ **108 KB** |
| SPU RAM | 35 KB used, 469 KB free (section 7.1) |
| voices | 4 music, 20 SFX |

---------------------------------------------------------------------------------------------------
## 8. Native platform and the trailer

* Native (`core/native/plat_native.c:161-173`): `plat_tone` / `plat_sample` only count calls (`nat_tone_calls`, `nat_sample_calls`, `nat_last_sample`), with an optional `--verbose` log. **No synthesis, no WAV, no audio hash.**
* Trailer (`trailer/music.c`, `trailer/tk.c`): an offline float synth at 22,050 Hz mono into `trailer.wav` (`tk.c:480, 536-584`; floats, `calloc`, `<math.h>`, so not portable to core). Its tools are worth porting as *data and ideas*:
  - mute and duck windows pinned to scene frames: the score is carved by comedy beats (`music.c:96-137`, `ducks()` at `1015`);
  - `RS()` tape-stop slides (`music.c:166-168`);
  - `SFX_SCRATCH` = noise swept 3000→400 Hz over 0.12 s, then 400→3000 Hz over 0.18 s (`tk.c:508`);
  - a "GBA-style echo" (a quieter 12.5 % pulse copy 3 sixteenths late, `music.c:272-273, 326, 346`);
  - sections pinned with `tk_scene_start()`;
  - its record scratch at the Wall (9.70 s) and the party (32.97 s) freezes into 0.47 s of silence before the DOOM riff (`music.c:20-40`).

## 9. Tests, determinism, tooling

* `core/tests/test_audio.c`: songs valid and ≥ 15 (`8`); cute songs never put a saw on the lead and metal songs do (`23`; it checks the *core* wave, not what GBA plays); title loops on the beat at 405-417 ticks (`56`); all 4 channels play (`80`); the volume option mutes (`104`); the stinger returns (`125`); SFX PCM within budget (`146`); babble follows the sliders (`175`); every song survives 1,500 ticks (`202`).
* `core/tests/test_studio.c`: steps land on exact ticks (`63`), ties/rests/singing (`98`), order/loop (`139`), choir rotates and the BGM is restored (`178`), bounded cost (`219`), save round-trip (`389`), chunk budget (`458`), deterministic templates and Inspire (`486, 533`).
* `core/tests/test_concert.c`: `concert_song_hash` determinism (`26-36`), the player plays and sings (`147`), export (`180`), full scene show (`346`).
* Generators: `make -C core check-gen` regenerates `sfx_data.c` byte-identically (integer-only `sfxgen.py`). `core/tools/xcheck.sh` compiles every core file with gcc, arm-none-eabi and mipsel at `-Werror`.
* Runner scripts that exercise audio paths: `core/tests/scripts/{studio,concert_*,library_jukebox,life_exorcism,story_*}.txt` (PNG shots only).
* GBA harness: `audio-reset` / `audio` / `expect-audio MIN` measure mGBA's real L/R output peak-to-peak (`gba/harness/harness.c:183-211, 442-455`). `profile N [FUNC]` gives cycle-accurate per-function costs. PS1: `audio_selftest()` (SPU read-back, envelope, ENDX; `spu.c:348-403`) in the engine-test disc. There is no PS1 audio capture.
* **Determinism rules** (`ARCHITECTURE.md:38-58`): C99, no floats, malloc or stdio; identical results on all three builds; bounded per-tick cost (the core gets ~6 ms of a GBA tick). Audio state is deterministic (integer, local RNGs). Gameplay depends on music *timing* only through tick-locked rhythm scenes (Exorcism, Concert / Studio events). No hash covers audio output today.

---------------------------------------------------------------------------------------------------
## 10. Known issues and quirks (all relevant to "flowing, non-repetitive")

1. `ASONG_ONCE` ignored → Victory loops (fact 6).
2. 10.5-32 s loops, no intros, outros, variations or layers. Combat music is a 10.5-12.8 s loop.
3. Hard cuts on every music change; the overworld re-checks every 3 s.
4. 30 Hz update granularity (arps 7.5 Hz cycle; uneven 16ths at 150-184 bpm); GBA catch-up bursts; PS1 slowdowns change the tempo.
5. GBA: saw → 25 % pulse on ch0/1; pulse "pitched drums" → periodic-noise buzz; volume change = channel restart (zipper and phase reset); the wave channel has 4 volume steps.
6. PS1: mono centre; key-ons not batched; SFX louder than music; tone voices drone through CD overlay loads.
7. The three sequencers use two volume curves (`/2400` vs `*mv/7`) and three separate drum implementations.
8. Babble steals the music's harmony channel on every syllable.
9. The current GBA mixer costs ~26 cycles per sample per voice and runs at 10.5 kHz mono; the docs' 3D fps exclude its cost.
10. PS1 keeps 48 KB of 8-bit PCM in RAM after it has been converted into SPU RAM, plus 20 KB of static audio staging buffers.
11. Naming hazard: "scratch" already means a memory buffer (`scene_scratch()`, `plat_save_scratch()`, `save_scratch.c`, `scene_scratch.c`). Name the record-scratch effect something else (e.g. `vinyl` / `scrub`).

---------------------------------------------------------------------------------------------------
## 11. Headroom

### 11.1 GBA: software mixer budget (frame = 280,896 cycles per vblank; percentages are of all CPU)

Standard rates that divide a frame exactly:

| rate Hz | TM reload | samples/vblank | DMA double buffer mono / stereo | 1 voice @ 8 c/s (hand ARM, est.) | 1 voice @ 26 c/s (current C, meas.) |
|---|---|---|---|---|---|
| 10,512 | 1596 | 176 | 352 / 704 B | 1,408 cyc = 0.50 % | 1.63 % |
| 13,379 | 1254 | 224 | 448 / 896 B | 1,792 = 0.64 % | 2.07 % |
| 15,768 | 1064 | 264 | 528 / 1,056 B | 2,112 = 0.75 % | 2.44 % |
| 18,157 | 924 | 304 | 608 / 1,216 B | 2,432 = 0.87 % | 2.81 % |
| 21,024 | 798 | 352 | 704 / 1,408 B | 2,816 = 1.00 % | 3.26 % |
| 26,758 | 627 | 448 | 896 / 1,792 B | 3,584 = 1.28 % | 4.15 % |
| 31,536 | 532 | 528 | 1,056 / 2,112 B | 4,224 = 1.50 % | 4.89 % |

Per-sample cost assumptions (**estimates** for a hand-written, unrolled IWRAM ARM loop; confirm with `profile`):

| part | cycles per sample |
|---|---|
| 8-bit sample or wavetable voice: `ldrb [base,pos,lsr#n]` + `add` + `mla` | ~7 (IWRAM table) to ~10 (ROM, random) |
| soft pulse with true PWM (phase compare) | ~7 |
| soft saw | ~5 |
| one-pole filter | +3-4 |
| hard sync | +2 |
| stereo by routing voices to L/C/R buses | ~0 extra per voice |
| output clip+pack | ~3 per channel |
| EWRAM echo/delay tap read + feedback write | ~10 per channel |
| scratch ring write | ~2-3 |
| scratch playback (variable-rate read with interpolation) | ~10-12 |

Voices that fit (stereo out 6 c/s, 8 c/s per voice; first figure = dry, second = with stereo echo + ring buffer write):

| CPU budget | 10,512 | 13,379 | 15,768 | 18,157 | 21,024 |
|---|---|---|---|---|---|
| 3 % (8.4k cyc/vbl) | 5 / 2 | 3 / 1 | 3 / 0 | 2 / 0 | 2 / 0 |
| 5 % (14.0k) | 9 / 6 | 7 / 4 | 5 / 3 | 5 / 2 | 4 / 1 |
| 8 % (22.5k) | 15 / 12 | 11 / 8 | 9 / 7 | 8 / 5 | 7 / 4 |
| 12 % (33.7k) | 23 / 20 | 18 / 15 | 15 / 12 | 13 / 10 | 11 / 8 |
| 20 % (56.2k) | 39 / 36 | 30 / 27 | 25 / 22 | 22 / 19 | 19 / 16 |

Reading the table against the requirements:

* **3D (≥ current fps):** the docs' fps are measured with the mixer idle, and the scenes sit on the 50 ms line. So the new audio has to be cost-neutral in 3D against today's reality: PSG music (~0.5 % of a tick) + the C mixer's SFX load (2.9-7.8 %).
  - Replacing the C mixer by a ~8 c/s ARM mixer frees about 2/3 of the SFX cost. At today's 2-voice SFX envelope (12.7k cycles, 4.5 %) that budget runs **~6 voices at 13,379 Hz stereo, or ~8 at 10,512 Hz**.
  - Anything beyond that must be paid by r3d savings or by an "audio LOD" profile per scene class (fewer soft voices and/or a lower rate while `plat_view3d` runs).
  - The PSG's 4 channels stay free in every case and cost nothing beyond register writes.
* **UI (≥ 20 fps):** frames may grow from ~33 ms to < 50 ms, so in principle 8-12 % (or more) is affordable on most screens. But dialogue typing (24 fps) and the keyboard (22.3 fps) are already near the line, and the babble runs during dialogue. Budget ~5 % there.
* **Pre-rendered streams cost almost no CPU:** DMA from ROM to a FIFO is ~1 cycle per byte (~0.1-0.2 % at 13-27 kB/s), and ROM has ~29.5 MB free (≈ 36 min at 13,379 Hz 8-bit mono). FIFO B + DMA2 + timer 1 are free for such a stream, or for stereo.
* **Music clock:** a timer-1 IRQ or a sub-tick schedule inside the VBlank mixer can give a 60-240 Hz driver tick for PSG arps and vibrato (each IRQ entry is ~100+ cycles). Rhythm scenes must still see tick-locked positions (fact 15).

Memory for these features (EWRAM free 7,680 B; IWRAM 316 B spare):

| item | size |
|---|---|
| stereo DMA buffers at 13,379 Hz | 896 B (EWRAM is fine as a DMA source) |
| hand mixer code + hot voice state | ~1.5-2.5 KB IWRAM; needs IWRAM freed: lowering the 6 KB stack reserve to 4 KB gives ~2 KB (high-water is 1,744 B), plus the current mixer's ~1.2 KB |
| echo / delay line, 8-bit | 13,379 Hz: 250 ms = 3.3 KB mono; at half rate (lo-fi) 1.7 KB |
| scratch ring, 8-bit mono, 13,379 Hz | 0.5 s = 6.7 KB, 1 s = 13.4 KB; at half rate 0.5 s = 3.3 KB |
| one ring shared by echo + scratch | 0.5 s half-rate = 3.3 KB |
| more `DoomoSong` fields (e.g. a 32-step x 4-pattern scratch lane) | 128 B x 6 book songs = 768 B EWRAM + save-chunk bytes (book ~5.3 of 8 KB) |
| wavetables and instrument samples | ROM, free (RAM cost 0) |

### 11.2 PS1: SPU headroom

* **Voices:** 24. Music can take 12-16 if SFX are capped at 8-12; today music uses 4.
* **SPU RAM:** 469 KB free for ADPCM chip waves (pulses at several duties, saw, tri, multi-cycle "FM-ish" and morph tables, drum and vocal-chop samples) and a reverb work area.
* **Reverb work-area sizes** (bytes, psx-spx table): Room 0x26C0 (9.7 KB), Studio S 0x1F40 (7.8 KB), Studio M 0x4840 (18 KB), Studio L 0x6FE0 (28 KB), Hall 0xADE0 (43.5 KB), Half Echo 0x3C00 (15 KB), Space Echo 0xF6C0 (61.7 KB), Chaos Echo / Delay 0x18040 (96 KB). The reserved 8 KB fits only Studio S, so `SPU_SFX_END` (`spu.c:29`) must move down. All presets fit in the free space.
* **CPU:** a 16-24-voice sequencer + per-tick register writes is estimated at ~5-10k cycles per tick (< 1 % of a frame).
* **Main RAM:** ~75 KB above the stack reserve. Moving `sfx_data` out of resident RAM (build-time ADPCM loaded from CD at boot, then dropped) and sharing the 20 KB staging buffers (e.g. with the display-list scratch) would return ~65 KB. Scene-specific audio code can live in an overlay group (Studio / Concert slots have ~60 KB spare); the effect engine itself must be resident, because stings fire in every scene.
* **Real-time encoding:** with the current encoder, producing ADPCM on the fly is too slow (~500-800 cycles per sample). A dedicated single-filter "streaming" encoder (estimated ~40-60 cycles per sample) makes 0.5 s at 11,025 Hz ≈ 0.3 M cycles ≈ 9 ms.

---------------------------------------------------------------------------------------------------
## 12. Scratch feasibility facts (constraints for the design, not a design)

* **Neither platform can reverse a playing PSG or SPU voice.**
  - GBA PSG: pitch can be bent and the channel muted. A square or triangle single cycle sounds the same backwards; only the envelope direction differs.
  - SPU: ADPCM only plays forwards. The pitch register (0..0x3FFF, 4.12) can be swept every tick or vblank for tape-stop and speed wobble on all music voices for ~24 register writes.
* **GBA mix grab:** only the software-mixed part exists as PCM. The ring buffer must be written by the mixer (sizes in 11.1); the PSG is not capturable. Options:
  - render the soft voices backwards (negative phase increments);
  - put the "grab-able" music layers in the soft mixer;
  - re-render the past deterministically from a sequencer snapshot (the core sequencer state `Chan ch[4]` is 320 B, `audio.c:60-83`).
* **PS1 mix grab:** the hardware capture holds only CD L/R and voices 1 and 3 (512 samples ≈ 11.6 ms each), never the full mix.
  - A CPU-side mirror render needs ~15-25 cycles per sample per voice (estimate). Continuous 8 voices at 11 kHz is ≈ 5 % CPU; rendering on demand from a snapshot is ≈ 26 ms one-off for 0.5 s.
  - The rendered buffer must then be ADPCM-encoded (a streaming encoder, 11.2) and played on a free voice. Backwards = encode the buffer in reverse.
  - Sequencer-level scrubbing (pitch registers + re-keying in reverse order) is free.
* **Masking loads (PS1):** CD overlay loads already freeze the music for 0.3-0.9 s with voices droning. A tape-stop or scratch driven from a VSync callback (not installed today) could cover them. Comedic smash cuts that switch overlay group stall the CPU mid-gag (`../story/story_map.md` §1.3).
* **Crackle and noise bursts:**
  - GBA: noise channel (LFSR modes) or soft noise in the mixer;
  - PS1: noise-generator voice (`SPU_NOISE_MODE`) or the existing noise loop sample.

## 13. Hooks where music events can attach (existing code)

* Scene entry and exit `audio_music()` calls (section 2.7 table). Overworld `ow_music_for_map` (`overworld.c:138-150`) + 90-tick poll. Story beats `BEAT_C1_WALL`, `BEAT_C2_*` (`overworld.c:146-147`).
* Levels hell meter `combat_s.hell` vs `hell_at`, plus boss alive/dead (`levels.c:540-555`): a natural intensity parameter for dynamic layers. The arena's horde and boss rush (`arena_modes.c:166, 230`).
* Cutscene panels: `cs_update` ticks `ct` / `pt` per panel, with hard-coded SFX cue points (`cutscene.c:108-129`) and the music per cutscene (`92-105`). The story map's planned comic-timing / timeline layer is where scratch stings belong (`../story/story_map.md` §15, §18).
* Dialogue typing: per-char babble (`ui_dialog.c:170-177`). The custom spec wants ducking under dialogue (AU22) and babble styles (AU16-AU19, A33-A34).
* Concert `PEV_*` events and Studio `song_beat_age()` / `song_pos_*()` (`song.h:102-160`). The spinning-record UI in the Studio SONG card and the Jukebox is a natural DJ / scratch surface.
* Exorcism rhythm at `EXORCISM_BPM` (`exorcism.h:32`, `exorcism_scene.c:176-189`).
* Custom options spec (`../custom/wishlist.md:896-921`): AU01 master, AU02-AU08 buses, AU09 stereo/mono/swapped, AU10 GBA speaker EQ, **AU11 PS1 SPU reverb (Off / Room / Studio / Hall / Space echo)**, AU12 reverb amount, AU13 location music, **AU14 invasion music (Metal / Cute remix / Keep theme)**, **AU15 dynamic combat layers**, **AU21 GBA mixing rate (10 / 13 / 18 kHz)**, **AU22 duck music under dialogue**. `OPT_COUNT` is capped at 64 (`town.h:121`, ~57 used); the spec proposes an `OPTX` chunk.

## 14. Files I produced (scratch only)

* `music/build/songstat.c`: links the real `audio.c` + `songs.c` on the host; prints the per-song table in 2.7.
* `music/build/gba/prof_et.txt` + `.out`, `prof_title2.txt` + `.out` (and `prof_title.txt`, top-25 list only): harness profiles behind 2.8 and 6.2 (the ET profile reproduces exactly: 823,817 cycles, IRQ 4.7 %). They ran on copies of the 16:32 ROMs with `scratchpad/perf/g-t2e/harness`.
* `music/build/gba_*.o`, `ps1_*.o`, `irq.iwram.s`: size measurements and the mixer's generated ARM code.
