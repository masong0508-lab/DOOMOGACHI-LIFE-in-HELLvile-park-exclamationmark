# Lead brief: stream STUDIO

Standing brief of the session that leads the STUDIO stream (`../PARALLEL_WAVES.md`). A new or restarted
lead reads it and follows it.

Owner, 2026-11-15: "vastly improve the song studio to be more user friendly and intuitive at the same time,
with particularly detailed tool functionality and playability, and the possibility to have advanced options to
make full-fledged songs". The decision with Claude's defaults: `../OWNER_DECISIONS.md`, "SONG STUDIO, FRIENDLY
AND FULL". Nobody watches this session: never stop to ask; take the plan's default, write it into
OWNER_DECISIONS.md as "default chosen by Claude (STUDIO), owner may override", and go on.

## Read first
1. CLAUDE.md; `../PARALLEL_WAVES.md` (streams, file ownership, budgets in section 3, landing in section 4 with
   steps 3b and 3c: the land loop of `tools/agents/wave-runner.js` and the `LANDING_HOLD` file).
2. `../OWNER_DECISIONS.md` (latest entry wins), `docs/STUDIO.md` (the studio as built: tabs, controls,
   templates, composer, format, persistence, budgets, the GBA speed numbers), `docs/MUSIC_STUDIO_ROADMAP.md`,
   `docs/AUDIO.md` and `docs/design/music/direction.md` (the music engine and its tiers per platform).
3. `../status/STUDIO.md` (yours: keep it current).

## The plan (packages of about a day each; rounds of two or three with wave-runner.js)
1. **ST-0 measure and design:** play every tab on GBA, PS1, Dreamcast; list what is confusing (one line
   each), what is missing, and the fps of every studio window. Write `docs/design/STUDIO_PLAN.md`: the new
   flow (Easy by default, Advanced behind a switch), the screens, the controls per platform, the song
   format changes (versioned: old songs load), the memory per platform. Land it first.
2. **ST-EASY:** a guided start (style, mood, tempo, length: a full working song in seconds from the
   composer), one instruction line and a control card on every tab, clear names, undo / redo, play from
   anywhere, the cursor's item described.
3. **ST-PLAY:** jam mode (the face buttons and d-pad play the song's scale and chords, no wrong notes),
   live recording into the tracker with quantize, loop playback while editing, a drum-pad mode.
4. **ST-TOOLS:** the note grid with selection, copy / paste / transpose / repeat, per-track instrument and
   envelope, volume and pan, swing, a chord track, a drum pattern editor.
5. **ST-ADV:** the Advanced switch: sections (intro, verse, pre-chorus, chorus, bridge, outro) arranged
   freely, key and tempo changes, automation, as many tracks as each platform's audio engine allows, songs
   of several minutes, lyrics and choir across the whole song.
6. **ST-USE:** the Concert Hall, PERFORM and the show's songs play user songs; the studio's look
   decluttered and beautified (one style, fewer things at once; owner decision of 2026-11-15).
Each package: the 30 fps rule (measure the studio windows with speed2 before and after), the memory gates,
old saves load, tests for every new rule, before / after screenshots, docs (`docs/STUDIO.md`, ROADMAP).

## Files
Yours: `core/src/studio*.c`, `song_book.c`, `song_player.c`, `songs.c`, `concert_song.c`, `concert_lyrics.c`,
`unlocks_studio.c`, `core/include/doomo/song.h` and the studio headers, `docs/STUDIO.md`,
`docs/MUSIC_STUDIO_ROADMAP.md`. The audio engines (`gba/src/audio.c`, `ps1/src` SPU code, `dc/src` sound)
belong to SPEED / DC: the smallest change you need, marked `cross-stream:`. The SPEED stream's S3-8 package
leaves the studio STAGE tab to you.

## Machine and running the stream
A fresh container: `sh tools/dev_setup.sh apt`, then in the background `JOBS=2 sh tools/dev_setup.sh psn00bsdk
psxavenc redux` and `JOBS=2 sh tools/dev_setup.sh kos flycast` (about an hour). Use the Workflow tool with
`tools/agents/wave-runner.js` (stream "STUDIO"); without it, the Agent tool with the same three steps; if a
permission check refuses both, stop and message the coordinator (session_01RLcGbHBa2kju7ynbaJLHT6); never work
around it. At most about five agents busy. A check-in with send_later every 40 minutes while agents run. One
push per package. Message the coordinator only for a blocker another stream must solve, an owner decision, or
when the plan is done. Never force-push, push only to claude/nice-allen-gr6czm, no pull requests. The hard
content rules of CLAUDE.md bind every lyric, template and song name.
