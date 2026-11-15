# UI stream: paused 2026-11-15 (coordinator PAUSE, weekly allowance)

Nothing of the UI stream's packages has landed yet except UI-0 (`c99a1e2`). Two packages are built and committed
(not reviewed, not landed), saved as `git format-patch` series:

- `ui-fit/`     UI-FIT: the text-fit gate in `make test`, weak no-op gate API, status with measurements (2 commits).
- `ui-options/` UI-OPTIONS: Options rebuilt (nine tabs, groups, one-line help, detail card, Y reset, previews,
  page memory), docs, ledger row, speed2 options group, text-fit hook (3 commits). It builds on UI-FIT's gate.

To resume: `git fetch`, rebase-apply with `git am` (ui-fit first, then ui-options; on conflict keep both sides),
run all gates (`tools/agents/wave-runner.js` GATES) on the rebased tree, then run the review and land steps
(wave-runner.js with `build`/`review` omitted is not possible: pass the packages again and let it rebuild from
these branches, or land by hand with an independent review). Respect `LANDING_HOLD`. Toolchain setup
(`sh tools/dev_setup.sh redux`, then `kos flycast`) was killed mid-build: restart it (JOBS=2, detached with setsid).
Next after these: UI-MENUS (pause, shops, items, roster, news, unlocks, quest log, hub, save/load, title), then
UI-POLISH (docs/design/UI_PLAN.md section 5). GBA EWRAM / PS1 resident share: 0 B.
