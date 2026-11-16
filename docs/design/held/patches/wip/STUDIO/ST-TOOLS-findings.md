# ST-TOOLS findings (2026-11-16): what is still left

Done: see `git log --grep ST-TOOLS`, docs/STUDIO.md "ST-TOOLS" (both passes) and the ROADMAP Done line. The independent
review of the first three commits found the mix helper drift, the chord suggester's "before" (order list, pentatonic start),
a cut-off message, plurals, a mark carried between patterns; all fixed with tests (section "second pass" of docs/STUDIO.md).

Still left, all because they need song bytes the classic 892-byte song does not have (ST-ADV's `'SNG2'` chunk and the player
that reads it, `design/STUDIO_PLAN.md` section 3):

- per-track envelope presets, per-track pan, per-track swing (the song has one swing; instrument and volume per track are on the
  SONG tab already);
- a true lane mute in the drum editor (the lanes share one drums cell per step; "Clear this lane" is what it offers);
- the chord lane as a fifth track and "follow chords" styles (the chord track screen edits the bass root and harmony third);
- the piano-roll note grid and a rectangle selection across tracks.

Open for the lander: the PS1 / Dreamcast gate numbers are in the landing commit message.
