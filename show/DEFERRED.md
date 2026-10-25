# show/DEFERRED.md - what the pilot does not build, and why (one line each)

- ART cast: Mr. Flops as a floppy-eared bunny flipbook (ACC_TEDDY stands in; nothing in 101 turns on his ears).
- ART cast: a per-character viseme table (series/visemes.shw is AUDIO's; the cast sheets show the sec.9.3 defaults and every speaker's mouth was sized to read with them).
- ART cast: the PS1-rendered model sheets (`show art sheet` on show.exe, AT6.1) until the player draws busts; the host sheets use the same engine code.
- ART cast: arm poses as flipbooks (Mascotty's quarter-speed wave, the Mayor's ladder/roller/reach, Kevin's lone third-eye blink): the portrait engine draws none; staging covers them with head turns, expressions and cutaways until a flipbook exists.
- ART cast: the pet and the Puff Demons have no model sheet here (pet_render / the sprite engine, not portrait.c); cast.shw maps them (pet:0, pet:-1, pet:12, SPR_PUFF_DEMON).
- ART cast: generic rule-of-three and whiplash-insert macros (shot structure, `insert`/`replay=` headers, not an actor's acting).
- ENGINE: `show vault push` and the remote vault (private repo / orphan branch / release assets): the creator decides where the vault lives; the archives wait in `<scratch>/vault/`.
- ENGINE: packing the TTS/ASR models into the vault (~1.2 GB): pinned by hash in `show/vault.lock` this week, packed after the release (disk is tight).
- ENGINE: `show/hooks/session_start.sh` (the opt-in restore hook): `show vault fetch DIR` restores by hand; nothing in this week's container needs restoring.
- ENGINE: `show/tools/requirements.lock`: the tools use what this container has (numpy, Pillow, pytest); `show doctor` checks them.
- ENGINE: gaze hunk (e), `ps1-show-wide`, `ps1-show-dev` (PCdrv), `runner-show`, the P2 draw trace (`show_wrap.c`): deferred to 2027 by the spec itself (§1.2, §4.3, §8.8).
- AUDIO voice: MFCC-DTW alignment of recordings against the TTS render (spec §1.2): Whisper word times + TTS phone ratios, energy fallback.
- AUDIO voice: espeak-ng G2P fallback (its wheel's data path is broken here, spec R7): cmudict + lexicon.shw + compound splitting cover every pilot word.
- AUDIO voice: lib:pet/beep_v1 is the game's piezo, an SFX (WP10 library), not a voice take; mt7_pet_01 and the pet lines lock as `pending`.
- AUDIO voice: stressed words in CAPITALS (WORD, SPEECH, PLAY) get no extra emphasis in TTS; the creator's reads carry it.
- AUDIO voice: retime="fit" time-stretching of recordings (world.stretch exists; the compiler decides when a take overruns its slot).
