# Stream DC: status

Kept by the stream's lead (rules: ../PARALLEL_WAVES.md section 5). Update it with every landing.
What goes in: last update, lead session, landed (package, commit, key numbers, PS1 margin and
delta, GBA sizes), in flight (package and stage), next up, blockers, needs from other streams.

- **Last update:** 2026-11-13 (DC5B landed)
- **Lead:** session_01Xd63czSpihgJ8Rcfy2jCub (its own cloud machine; every toolchain and emulator installed: GBA, PS1 SDK + PCSX-Redux, KallistiOS + Flycast)
- **Landed:** landed before the restart: DC2, DC3, DC4, DC5, DC7, DC8. **DC5B (this session, sha in the push below):** the music engine's IRQ tier (`dc/src/musvoice.c`, `mixer.c`, docs/DREAMCAST.md section 6). Flycast (`make -C dc audio-test`): title r 0.990, invasion 0.997, combat 0.822 (gated at 0.8 on this tier; the bridge image had 0.904), 0 ticks over 33.3 ms, 0 sound underruns / late refills, repeat hash equal; mixer 1.2-1.4 ms per tick mean (no more than the bridge). `make -C dc` and `MUSIC_IRQ=0` both link. Host: test_audio 2046, test_tier 73, 28 songs on both tiers, core 1067 passed (SAN=1 too, only the known UBSan reports), GBA build+test, .iwram 11,256 B / .iwram_bss 4,084 B unchanged, `audio_channel_state` is a weak alias (no byte on any port). PS1 not built (no SDK on that machine): the core change is zero bytes, the margin is as on the base. Found and fixed in the review of the draft: the babble hand-back put the harmony on tone channel 1 twice; the IRQ tier is not the bridge's sound by design (docs section 6). Open: the music no longer ducks under the babble and one-shots (listen on a console), the RAM ledger is about +370 B, the 2 ui-compare shots options_cheats / opt_5_perf fail on the base too (Draw distance row, not DC5B).
- **In flight (round 1, restarted 04:55 UTC, building; the worktrees survived the pause):**
  - DC6A: `plat_view3d` on the PVR, part 1 of 3 (walls, floors, ceilings, sky; host parity test, `make -C dc v3d-test`). Draft from before the pause: `../patches/wip/DC/fut-dc6a.uncommitted.diff` (deleted when it lands) (part 1 committed on branch fut-dc6a: host parity test passes, KOS build and Flycast run still owed)
  - DC5B: the music engine's IRQ tier on the Dreamcast. Draft: `../patches/wip/DC/fut-dc5b.uncommitted.diff` (deleted when it lands)
  - STKOPT: the Options row "Stick: turn / strafe". The second option table already exists (`OPT2_FIRST` = 64, save chunk 'OPT2'): the package adds one id at its end. Other streams that need a new option do the same.
- **Next up:** DC6B (billboards, weapons, effects, HUD), DC6C (`OPT_DRAW_DIST`, combat detail option, governor, Max detail at 30); then DC9 by scene family; TD1-TD3 after TK5
- **Blockers:** none
- **Needs from other streams:** none. For LEVELS: DC6 has not landed yet (this file will say when).
