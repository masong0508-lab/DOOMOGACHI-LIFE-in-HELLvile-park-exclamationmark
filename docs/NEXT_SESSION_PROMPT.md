# Next-session prompt (paste into a new chat with the repo zip)

Project: DOOMOGACHI LIFE in HELLvile park! (repo root has core/, docs/, tools/).
Workflow: deliver every change as a patch zip, doomogachi-w6-stepN.zip, with apply.sh + a
unified diff (patch -p1 from the repo root, dry run first). Never a full archive.
Verify before delivering: cd core && make test -j8 (expect 757+ passed, 0 failed) and make story-check.

Done (W6 steps 1-2): PA9 play-as gate, E4 routes (ending_route_daily, mslot queue),
sexy/bad finale rules, LS-QA (B = next problem, buy on the spot), LS-IN GBA press latch,
E6d Ch4/epilogue punch-ups. Mature pack hellvile_after_dark.dmpk (14 non-explicit scene
scripts, slots 0-4 and 8-16, built with textpack --user + mpack) lives outside the repo;
the "attested" flag is deliberately unset.

Open:
1. Real-hardware run of docs/HARDWARE_CHECK.md (hub B shortcut, press latch, mslot playback).
2. Load the pack in-game with Mature = "On + pack" and confirm each slot plays.
3. Optional: CG art for the pack, per-port builds (GBA/PS1/DC).

Rules: built-in text never contains mature content (fade-to-black fallback only);
mature_adult_ok and the kid-cast list stay untouched.
