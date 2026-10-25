# DOOMOGACHI LIFE in HELLvile Park!
## Episode 101, "Mingle Night": Recording Guide (voice acting at home)

This guide is for you, the creator, recording some or all of the pilot's voices yourself. Every line you don't record plays in a temporary TTS voice (stock Kokoro voices only), so you can start with one character and add more later. A recording replaces the TTS for that line, by its line ID.

| You need | Where it is |
|---|---|
| The script | `PILOT_SCRIPT.md` (read the scene before you record its lines) |
| Your lines, in order, with cues and target lengths | `sides/<ROLE>.md`, one file per role (`MAYOR_GIGGS.md`, `PIP.md`, `FATHER_FUZZ.md`, and so on) |
| How each voice sounds, and who can double whom | `VOICE_BIBLE.md` |
| Every line in one table | `lines.csv` (`line_id, scene, character, text, direction, target_seconds`) |

---

## 1. What to record first

You don't have to voice everyone. The episode has **165 line IDs**: 137 cast lines, 6 pet beeps and 22 walla (crowd) lines.

| Plan | Record | Leave on TTS |
|---|---|---|
| **Start here** | **Mayor Giggs** (35 IDs, 33 recordings: he carries the comedy), then **Pip** (16: the climax), then **Father Fuzz** (16) or **Nana Puddin** (22) | Everyone else |
| **Go further** | Add Bonejangles (15), Steamy (5, with your own lilt or a fictional one, never an imitation of a real accent), Kevin (3), Hattie Haha (3), Miss Lulla (1), Dr. Bloom (5) | Clinker, GURGLE, the walla |
| **Everything you can** | All of the above. GURGLE and the purr (4) only if you will also voice Grumbleguts from Ep 102, because they are always one performer | Clinker, the walla |

**Fine to leave to TTS** (the temp voice is close to final for these, because they are processed, stacked or mixed low):
* **Clinker** (all 12 IDs: `a1_14_clink_01`, the ten `a2_*_clink_*` / `a3_07_clink_01` twins and `a4_03_clink_01`). Her voice is laid 8 frames under the possessed Bonejangles, and it only reads as two people with two different voices. If you voice Bonejangles, leave Clinker on TTS.
* **All 22 walla lines** (`w_kids_01..06`, `w_tent_01..02`, `w_wall_01`, `w_giggle_01..11`, `w_fog_01`, `w_fog_02`). See `sides/WALLA.md` if you want to stack your own reads in.
* **The fog's child "...us"** (`w_fog_02`): it must **never** be voiced by Pip's performer. Leave it on TTS if you voice Pip.
* **GURGLE and the purr** (`pc1_gurgle_01..03`, `pc1_grum_01`), unless you plan to voice Grumbleguts.
* **Kevin's "It's lonely."** (`a1_05_kevin_03`) is optional and the first cut if 1-05 runs long.

**Never recorded by anyone:** Sunny (silent: no lines, no breaths), the pet's six "beep."s (the game's raw piezo sound), Mascotty (silent), the fountain's two giggles (sound-design library takes), and the main title song (the theme's own singer, not a cast role).

---

## 2. Your setup

**A phone or a USB mic is enough.**
* **Phone:** use a recorder app that saves **uncompressed WAV** (not the built-in voice-memo default, which is usually compressed .m4a). Turn on airplane mode so nothing buzzes or rings. Hold it or stand it with the mic end pointing at your mouth, slightly to one side.
* **USB mic:** any cardioid USB mic into a free recorder such as Audacity. Set the project rate to 48000 Hz and record in mono.
* **Headphones** (any wired pair) for the lines you record against something: Clinker in sync with Bonejangles, the hums against the music-box cue, and the sung lines against a click.
* **A pop filter**, or a thin sock stretched over a bent coat hanger, a few centimetres in front of the mic.

**The room.**
* The quietest, softest room you have. A wardrobe full of clothes, or a corner with a duvet hung behind you, beats a big empty room. Avoid bare walls, kitchens and bathrooms (they echo).
* Switch off the fridge, fans, heating or air conditioning while you record, if you can. Close the windows.
* At the start of each session, record **2 seconds of silence** in the room and save it as `roomtone_<YYYY-MM-DD>.wav`. The cleanup uses it.

**Settings.**
* **48 kHz, mono, WAV.** 24-bit if your recorder offers it; 16-bit is fine from a phone.
* **Level:** your loudest words should peak between **-12 and -6 dBFS** (about two-thirds up the meter) and never clip. Do a test shout before Pip's "CLINKER!" and the battle cry, and turn the input down for those.
* **No effects at all:** no EQ, no compression, no noise reduction, no reverb, no "enhance". The show's ingest does the cleanup (high-pass, light denoise, trim, loudness to -23 LUFS) the same way for every take.

**Distance.** About a **fist's width (10-15 cm)** from the mic, slightly off to one side so the "p"s and "b"s don't pop. Keep the same position all session. For shouts (the battle cry, "CLINKER!", the stampede) lean back to about 30 cm rather than turning the input down mid-session.

---

## 3. How to record each line

* **One line ID per file.** If the script splits a speech with a gap (`[R8]`, `[R13]` and so on), each piece is its own ID and its own file. Leave out the pause: the engine places the gap to the frame.
* **Three takes per line**, each its own file (`_t1`, `_t2`, `_t3`). Vary them a little: one as written, one bigger, one smaller.
* **No slate.** Don't say the line ID or "take two" on the recording: the file name says it.
* **Half a second of room tone** before and after each take. Don't trim it; the ingest trims.
* **Aim near the target length** in the sides. The engine can stretch or squeeze a take by up to 15% to fit its slot (`retime = "fit"`). The lines locked to music or to the frame (the cold open's dawn lines, the possessed ratings and couplet in 2-01 and 2-02, the 4-03 couplet, the hymns) need to land close.
* **The codes in the text:** `{d}` deadpan (flat, no smile) · `{w}` wavy (the fog) · `{big}...{/}` the word lands whole · `{..}` a 0.27 s pause inside the line · CAPITALS mean stressed, not shouted (Pip's "CLINKER!" is the one real shout) · `...` at the start of a line means come in a little late and small.

**The special lines.**

| Line | How to do it |
|---|---|
| **"Hee. Hee."** (the Mayor; `co13_heehee`, `a1_08_mayor_09`, `a1_13_mayor_01`) | One library take, reused all three times. Record about ten passes in one sitting: two falling notes a major third apart ("mi... do"), each about 0.4 s, with a short gap between. Pick one and save it as `heehee_v1.wav`. The flat "hee. hee." read in 1-12 (`a1_12_mayor_02`) is a different, ordinary line |
| **The Mayor's hums** (`co12_hum`, `a4_08_mayor_01`) | Hum the Mingle tune over the music-box cue in your headphones, starting on its held note and stopping dead on the muted note. The 4-08 hum is the same, exactly one note further. Save as `hum_dawn_v1.wav` and `hum_plusone_v1.wav`. If you'd rather not, TTS `sing()` renders them |
| **Possessed Bonejangles** (2-01 to 3-07) | To a 140 bpm click, one syllable a click, every word the same weight. Don't add a lag or an echo |
| **Clinker in sync** (if you do record her) | Play Bonejangles' take in your headphones and speak with it: same start, same rhythm. Never lag on purpose; the engine slides her exactly 8 frames. "...Seven." (`a4_03_clink_01`) is recorded alone, on time |
| **Fuzz's hymn** (`a2_06_fuzz_02`, `a3_07_fuzz_03`) | Sung to a 140 bpm click, clapping on 2 and 4 if you like. In 2-06 let it drift late until the last two words slip into minor; in 3-07 hold the tempo and stay in major |
| **Bonejangles' couplet** (`a4_03_bone_03`) | Spoken-sung to the hook at 140 bpm, warm, and let "pink" ring |
| **Fuzz's "Cl-"** (`a3_07_fuzz_02`) | Start the word and stop dead on the L, with no breath out. Record two or three versions |
| **Nana's "Cl..."** (`a4_01_nana_01`) | A sound that dies in her mouth, not a stutter |
| **Pip's "CLINKER!..."** (`a4_02_pip_03`) | The last take of the whole session: from the diaphragm, supported, no rasp; two or three takes at most. It plays dry and alone, so tell the ingest to skip denoise on it |
| **The Mayor's "Being calm!"** (`a2_03_mayor_02`) | Squeaky, as if from under a table, but don't muffle yourself: the mix does that |

---

## 4. File names

| What | Name | Example |
|---|---|---|
| A take | `<line ID>_t<n>.wav` | `a1_08_mayor_01_t1.wav`, `a1_08_mayor_01_t2.wav` |
| A rough guide read (optional, before the real session) | `<line ID>_s<n>.wav` | `a4_02_pip_03_s1.wav` |
| A walla line | `<line ID>_t<n>.wav` | `w_tent_01_t3.wav` |
| A library take | `<name>_v<n>.wav` | `heehee_v1.wav`, `hum_dawn_v1.wav`, `hum_plusone_v1.wav` |
| Room tone | `roomtone_<YYYY-MM-DD>.wav` | `roomtone_2026-10-26.wav` |

Line IDs are lowercase with underscores, exactly as they appear in the sides and in `lines.csv`.

---

## 5. Where to put the files

The show engine spec (`docs/show/SHOW_ENGINE_SPEC.md` §4.1, §5.6, §10.2) keeps recorded takes in **`show/lib/takes/<voice>/`**, and the script breakdown adds one episode level so ep102's IDs can't collide with these. Drop your WAVs here:

```
show/lib/takes/<voice>/e101/<line ID>_t<n>.wav
```

`<voice>` is the role's code: `mayor`, `nana`, `pip`, `fuzz`, `bonejangles`, `clinker`, `bloom`, `steamy`, `kevin`, `hattie`, `lulla`, `gurgle`, `grumbleguts`, or `walla` for crowd lines. Library takes go one level up, in the voice's own folder (`show/lib/takes/mayor/heehee_v1.wav`).

Examples:
* `show/lib/takes/mayor/e101/a1_08_mayor_01_t2.wav`
* `show/lib/takes/pip/e101/a4_02_pip_03_t1.wav`
* `show/lib/takes/mayor/heehee_v1.wav`

Then each take is ingested (`show voice ingest <wav> --line <line ID>`). The ingest cleans it up the same way every time and saves a `.flac` beside it, `show/lib/takes/<voice>/e101/<line ID>_t<n>.flac`, which is what the episode plays. If the `show` tool isn't set up yet, just drop the WAVs in the folders and ask Claude to ingest them.

**Don't commit the WAV or FLAC files to git.** The spec keeps audio binaries in the vault, with only their hashes in git (§10.7).

---

## 6. Marking your favourite take

The pick for each line lives in **`show/ep101/takes.toml`**, which is yours to edit. Add one entry per line you've recorded:

```toml
[a1_08_mayor_01]
take = "rec:show/lib/takes/mayor/e101/a1_08_mayor_01_t2.flac"   # your favourite
retime = "fit"      # keep the script's timing; stretch the take by up to 15% to fit
```

* A line with no entry plays in this order: your recording, then a guide read, then a library take, then TTS. So you only need an entry to choose *which* of your takes plays.
* `retime = "take"` instead lets your take's own length drive the timing (the show re-times the shot around it). Use it when a take you love runs more than 15% over its target.
* To go back to TTS for a line, set `take = "tts"`.
* **Quicker option:** while you record, keep a plain list of picks, one per line (`a1_08_mayor_01 t2`), and ask Claude to turn it into `takes.toml`.

---

## 7. A session

1. Warm up for ten minutes: humming, lip trills, a few sirens.
2. Record 2 s of room tone.
3. Library takes first, while your voice is fresh: "Hee. Hee." (ten passes) and the two hums.
4. Then one character at a time, in story order (the sides are already in order), so each voice stays consistent.
5. Loud and wet things last: Fuzz's battle cry, any walla shouts, Pip's cry in 4-02 and "CLINKER!" (the very last take), then GURGLE and the purr.
6. Drink water and take a break every 20 minutes. Stop if anything scratches: a strained voice is gone for the week.
7. Name every file as you go, and note your picks.

**Before you start each session, check:** airplane mode on, fridge and fans off, 48 kHz mono WAV, levels peaking at -12 to -6 dBFS, the same mic distance as last time, and headphones for any sync or sung lines.

**If you double roles:** keep each voice in its own pitch-and-pace cell (`VOICE_BIBLE.md` §5). The pairs to watch are Fuzz and Dr. Bloom (2-05, 3-04, 3-07: both high, so make Bloom slower and cooler, or leave her on TTS), the Mayor and Pip (keep Pip higher and faster), and Bonejangles and Clinker (always leave one of them to TTS or a second person).
