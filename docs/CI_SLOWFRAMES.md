# CI slow-frame report for the invaded map (2026-11-07)

`make -C gba profile-invaded` (run by a new CI step, "Invaded: what the over-budget frames spend extra time in" in the Summary tab,
log in the gba-speed-report artifact as `invaded_profile.log`) now prints, besides the plain top list:
- `slow frame tick T work X ms`: the first 40 frames over 33.3 ms (the gaps between ticks show the period of the dips),
- `slow+ D ms/frame NAME (slow frame A ms, normal frame B ms)`: the functions that cost the most MORE in a slow frame than in a normal one.
(`--worklog` turns it on; `gba/harness/harness.c`, `profile()`.)
