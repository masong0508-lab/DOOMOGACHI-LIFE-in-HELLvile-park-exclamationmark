# ST-TOOLS findings (2026-11-16)

Done (committed): see `git log --grep ST-TOOLS`.

- Baseline on this machine: `make -C core -j3 test` 1538 passed, 0 failed (tip 266e8acb).
- Step 1: `core/src/studio_tools.c` (pure range tools + the seven helper tools as functions on a DoomoSong) and
  `core/tests/test_studio_tools.c` (13 tests). Composer exports in `studio_gen.c` (groove names, `studio_phrase`).
  The composer's own output is unchanged (drum role 4 is new: a groove without its fill).

Left: see the bottom of this file as the work goes on.
