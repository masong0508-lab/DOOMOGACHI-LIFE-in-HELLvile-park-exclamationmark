# show/ep101/vo/: drop your own recordings here

The RECORDING_GUIDE.md folders work too: `show/lib/takes/<voice>/e101/<line id>_t<n>.wav`.

Any line can be your voice instead of the temp TTS, one line at a time (spec §5.6, §10.2).

1. Record the line (WAV, 48 kHz, 24-bit, mono is best; any rate works) and name it by its
   line id from `show/ep101/lines.shw`:

   | What | Name | Example |
   |---|---|---|
   | A take | `<line id>_take<n>.wav` (or `_t<n>.wav`) | `co15_perfect_take1.wav`, `co15_perfect_take2.wav` |
   | A scratch (guide) read | `<line id>_s<n>.wav` | `a4_02_pip_03_s1.wav` |

   About half a second of room tone before and after is fine; no effects, EQ or compression
   (the ingest does the cleanup). Peaks between -12 and -6 dBFS.

2. Drop the files in this folder (or run `show voice ingest my.wav --line co15_perfect`,
   which copies it here under the next take number).

3. Pick your favourite in `show/ep101/takes.toml` (optional):

   ```toml
   [co15_perfect]
   fav = 2              # use co15_perfect_take2.wav
   retime = "fit"       # keep the written timing (the take may be up to 15 % longer or shorter)
   ```

   With no `fav`, the highest take number wins. `take = "tts"` goes back to the temp voice
   for that line without deleting anything.

4. Run `show voice ep101`. Only the lines whose recording changed are processed: high-pass,
   light denoise, trimmed to 30 ms before the first sound, levelled to -23 LUFS, aligned with
   Whisper word times, and given a mouth track. Every other line stays exactly as it was.

Order when several exist for a line: a take > a scratch read > a library take > TTS.

The recordings themselves are not committed (this folder is gitignored, like all audio);
`show/ep101/voice.lock` keeps each file's sha256, and the vault keeps the files.
