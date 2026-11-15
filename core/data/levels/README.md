# core/data/levels: level text (owner: LEVELS)

One file per level, named like the map (`steam_room_panic.txt` for `core/data/maps/steam_room_panic.txt`).
It holds every line the level's script and its signs show. The text ids are the numbers the map's
`caption`, `bark` and `choice` statements use.

```
# comment
ID | text | clean alternate (optional)
```

* `\n` is a line break. At most 2 lines, at most 34 characters a line (240x160 screen).
* Markers after the id: `a` (adult line, needs the clean alternate), `j` (a joke line: dark levels need 2 to 4).
* The third column is shown when Mature content is off (towel gags become bathrobe gags, and so on).
  A line that needs one but has none fails the lint when its `adult` marker is set: `ID a | ...`.
* Text id ranges: level number n (LV_ index 7..30) uses `1000 + 100 * (n - 7)` to +99. HB8 = 1000..1099
  (see the file). 1099 of each range is the card line (the one plain rule line of the control card).
* Lint: `python3 tools/leveltext.py` (also `make -C core leveltext`): ids unique and in range, every id the
  map script uses exists, line lengths, a clean alternate where `adult` is set, and the hard content rules
  (no words from the banned list: see the script).
* English only until the Talk / i18n wave translates it (DECISIONS: written first, translated after).
