# Stream DC: status

Kept by the stream's lead (rules: ../PARALLEL_WAVES.md section 5). Update it with every landing.
What goes in: last update, lead session, landed (package, commit, key numbers, PS1 margin and
delta, GBA sizes), in flight (package and stage), next up, blockers, needs from other streams.

- **Last update:** 2026-11-12 23:55 UTC
- **Lead:** session_01Xd63czSpihgJ8Rcfy2jCub (its own cloud machine; toolchains installing since 23:40 UTC: PS1 about 00:20, KallistiOS + Flycast about 00:50)
- **Landed:** landed before the restart: DC2, DC3, DC4, DC5, DC7, DC8.
- **In flight (round 1, building):**
  - DC6A: `plat_view3d` on the PVR, part 1 of 3 (walls, floors, ceilings, sky; host parity test, `make -C dc v3d-test`)
  - DC5B: the music engine's IRQ tier on the Dreamcast (voice renderer, 240 Hz driver in the mixer, host reference render)
  - STKOPT: the Options row "Stick: turn / strafe". The second option table already exists (`OPT2_FIRST` = 64, save chunk 'OPT2'): this package adds one id at its end. Other streams that need a new option do the same.
- **Next up:** DC6B (billboards, weapons, effects, HUD), DC6C (`OPT_DRAW_DIST`, combat detail option, governor, Max detail at 30); then DC9 by scene family; TD1-TD3 after TK5
- **Blockers:** none
- **Needs from other streams:** none. For LEVELS: DC6 has not landed yet (this file will say when).
