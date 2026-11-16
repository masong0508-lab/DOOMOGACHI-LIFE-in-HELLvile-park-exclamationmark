# ST-TOOLS findings (2026-11-16)

Done (committed): see `git log --grep ST-TOOLS`.

- Baseline on this machine: `make -C core -j3 test` 1538 passed, 0 failed (tip 266e8acb).
- Step 1: `core/src/studio_tools.c` (pure range tools + the seven helper tools as functions on a DoomoSong) and
  `core/tests/test_studio_tools.c` (13 tests). Composer exports in `studio_gen.c` (groove names, `studio_phrase`).
  The composer's own output is unchanged (drum role 4 is new: a groove without its fill).

Left: see the bottom of this file as the work goes on.

## Pushed early by the owner (2026-11-16 ~01:05 UTC)
The three commits above were pushed at the owner's request before the independent review and before the PS1 / Dreamcast gates. Checked before the push: core tests (all passed), check-gen, the GBA build (sizes: .iwram 11,392 B, .iwram_bss 4,088 B, unchanged by ST-TOOLS). NOT yet done: the independent review, the PS1 build with the boot FMV + game_smoke + studio speed2, the Dreamcast build and speed2, the GBA speed2 studio groups with gates (the builder measured studio_tools at 30 on the GBA), and the rest of ST-TOOLS (finish the per-tool help lines and feedback, the drum pattern editor and chord track screens if not complete). Next session: review the pushed commits first, run those gates, fix what they find, then finish the package.
