# Stream DC: status

Kept by the stream's lead (rules: ../PARALLEL_WAVES.md section 5). Update it with every landing.
What goes in: last update, lead session, landed (package, commit, key numbers, PS1 margin and
delta, GBA sizes), in flight (package and stage), next up, blockers, needs from other streams.

- **Last update:** 2026-11-13 04:55 UTC - resumed (coordinator's RESUME)
- **Lead:** session_01Xd63czSpihgJ8Rcfy2jCub (its own cloud machine; every toolchain and emulator installed: GBA, PS1 SDK + PCSX-Redux, KallistiOS + Flycast)
- **Landed:** landed before the restart: DC2, DC3, DC4, DC5, DC7, DC8. Nothing landed in this session yet.
- **In flight (round 1, restarted 04:55 UTC, building; the worktrees survived the pause):**
  - DC6A: `plat_view3d` on the PVR, part 1 of 3 (walls, floors, ceilings, sky; host parity test, `make -C dc v3d-test`). Draft from before the pause: `../patches/wip/DC/fut-dc6a.uncommitted.diff` (deleted when it lands)
  - DC5B: the music engine's IRQ tier on the Dreamcast. Draft: `../patches/wip/DC/fut-dc5b.uncommitted.diff` (deleted when it lands)
  - STKOPT: the Options row "Stick: turn / strafe". The second option table already exists (`OPT2_FIRST` = 64, save chunk 'OPT2'): the package adds one id at its end. Other streams that need a new option do the same.
- **Next up:** DC6B (billboards, weapons, effects, HUD), DC6C (`OPT_DRAW_DIST`, combat detail option, governor, Max detail at 30); then DC9 by scene family; TD1-TD3 after TK5
- **Blockers:** none
- **Needs from other streams:** none. For LEVELS: DC6 has not landed yet (this file will say when).
