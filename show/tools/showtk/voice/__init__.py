"""showtk.voice: dialogue for the show (spec §9.3, §10.2-§10.3, WP9).

text -> phonemes (g2p, lexicon) -> take (Kokoro TTS + WORLD shaping, or the creator's
recording) -> align -> mouth track -> Whisper QA -> voice.lock (with provenance).

Run with `show voice ...` once the CLI lands, or `python3 -m showtk.voice ...` from
show/tools. See show/tools/showtk/voice/README.md.
"""
