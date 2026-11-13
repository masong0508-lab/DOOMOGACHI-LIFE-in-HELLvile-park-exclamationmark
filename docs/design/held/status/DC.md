# Stream DC: status

Kept by the stream's lead (rules: ../PARALLEL_WAVES.md section 5). Update it with every landing.
What goes in: last update, lead session, landed (package, commit, key numbers, PS1 margin and
delta, GBA sizes), in flight (package and stage), next up, blockers, needs from other streams.

- **Last update:** 2026-11-13 00:03 UTC - **PAUSED** (coordinator's PAUSE, owner at 96% of the hourly limit)
- **Lead:** session_01Xd63czSpihgJ8Rcfy2jCub (its own cloud machine)
- **Landed:** landed before the restart: DC2, DC3, DC4, DC5, DC7, DC8. Nothing landed in this session yet.
- **Stopped mid-build (round 1, workflow run wf_ac1d8947-454, about 12 minutes in; no commits yet):**
  - DC6A (`plat_view3d` part 1: walls, floors, ceilings, sky): uncommitted work saved in `../patches/wip/DC/fut-dc6a.uncommitted.diff`
  - DC5B (music IRQ tier on the Dreamcast): uncommitted work saved in `../patches/wip/DC/fut-dc5b.uncommitted.diff`
  - STKOPT (Options row "Stick: turn / strafe"): nothing written yet. The second option table already exists (`OPT2_FIRST` = 64, save chunk 'OPT2'): the package adds one id at its end.
  - Both diffs apply to base `de02ef4` (`../patches/wip/DC/BASES.txt`). They are unfinished and untested first drafts.
- **To resume:** if this machine still has the worktrees `.claude/worktrees/fut-dc6a` / `fut-dc5b`, re-run the workflow with the same arguments (resume run wf_ac1d8947-454; the build agents continue in the existing worktrees). On a rebuilt machine: `sh tools/dev_setup.sh` first, create the worktrees, `git apply` the diffs, then run round 1 again with the briefs (they are in the lead session's transcript; the scope of each is in the lines above and in `../leads/DC.md`).
- **Machine:** apt stage done; the PS1 and KallistiOS + Flycast setup jobs were still building at the pause (they use no usage allowance and were left to finish).
- **Next up:** round 1 again, then DC6B (billboards, weapons, effects, HUD), DC6C (`OPT_DRAW_DIST`, combat detail option, governor); DC9 by scene family; TD1-TD3 after TK5
- **Blockers:** none
- **Needs from other streams:** none. For LEVELS: DC6 has not landed yet.
