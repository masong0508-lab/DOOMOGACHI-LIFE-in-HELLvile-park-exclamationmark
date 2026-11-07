# V14b: the governor must come back (2026-11-07)
CI run #497: story map 29.8 fps / 31.1 ms and hahaland 30.0 / 24.6 ms (as intended), invaded unchanged (f87e5596), but park, hellvile and
map walk also changed hash and lost their sprites' cost (park 26.0 -> 20.5 ms): a scene's cold-start frames (cold tile cache, over 29 ms
three times in a row) switched the governor on, and it never switched off. On the way back the budget grew from `gov_kept` (what whole
distance buckets kept); when the next bucket is bigger than the step (1/8 + 128 px) the kept area, and so the budget, never grows.
Fix (non-red maps only; the red map keeps V10's rule, hash f87e5596): grow from `gov_budget` itself. Expected: park ad13740c,
hellvile bd930205, map walk 727f51de (the V13b run's hashes), story map and hahaland stay near 30 fps.
`invaded_profile.txt` now profiles 48M instructions (was 12M: 73 drawn frames, none over 33.3 ms) so the slow-frame report sees the dips.
