export const meta = {
  name: 'doomogachi-story-house-build',
  description: 'Build the story/dream-house/comic-timing SPEC in waves (S0..S5, H1..H4, I9), each package verified and committed',
  phases: [
    { title: 'Wave 1', detail: 'S0 contracts, then S1 M1 + H2/H3 M0' },
    { title: 'Wave 2', detail: 'S1 rest + S2 engine | H1 M1 + H4 M1' },
    { title: 'Wave 3', detail: 'S2/S3 content | H2/H3 renderers + H1/H4 rest | S4/S5 side missions' },
    { title: 'Wave 4', detail: 'S5 Feast port + I9 integration' },
  ],
}
const REPO = '/home/user/DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark'
const BASE = `Repo: ${REPO} (DOOMOGACHI LIFE: C99 core in core/, GBA port gba/, PS1 port ps1/ via PSn00bSDK at /opt/psn00bsdk; export PSN00BSDK_LIBS=/opt/psn00bsdk/lib/libpsn00b). Read ARCHITECTURE.md and the implementation SPEC docs/design/story/SPEC.md (its §12 "Critique fixes" is binding and overrides §1-§11), plus docs/design/story/story_final.md, housing_final.md, comic_timing.md and GRUMBLEGUTS_AND_DIRECTOR_NOTES.md (director's notes: must be honoured; keep the art cartoon-modest as already built). The feast cutscene (core/src/story_feast.c, core/src/grumbleguts_gfx.c, CUT_FEAST_POP) is already built to concept-sheet quality: keep and reuse it.
You implement ONE work package of the SPEC (named below), within its file ownership (§6, §12). Other packages run before/after you in the same tree; do not edit files you don't own except the listed shared-file hunks.
VERIFY before finishing (all must pass): make -C core test -j8; make -C core test SAN=1 BUILD=build-san -j8; sh core/tools/xcheck.sh; make -C core check-gen; make -C gba -j8 && make -C gba engine-test && make -C gba test; cd ps1 && cmake --preset ps1 && cmake --build --preset ps1 (must leave stack room; respect the SPEC's RAM ledger) && cmake --preset ps1-enginetest && cmake --build --preset ps1-enginetest. LOOK at runner screenshots of anything you draw (240x160 and 320x240).
Then COMMIT AND PUSH your package yourself so work is never lost to usage limits: git fetch origin claude/nice-allen-gr6czm; if the remote moved, git pull --no-rebase origin claude/nice-allen-gr6czm and re-verify; git add only your package's files (never build dirs or screenshots); commit message "<PKG>: <summary>" ending with the two lines:
Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01RLcGbHBa2kju7ynbaJLHT6
then git push origin HEAD:claude/nice-allen-gr6czm (retry up to 4 times with backoff on network errors).
If a previous attempt of this package was interrupted, git log and the tree will show partial work: continue it, don't redo it.
Return a short report: what shipped, files, sizes/RAM/fps numbers, verification results, commit hash, and anything deferred.`
const pkg = (id, extra, phase) => agent(`${BASE}\nYOUR PACKAGE: ${id}. ${extra || ''}`, { label: id, phase })

phase('Wave 1')
const s0 = await pkg('S0 (contracts and registries)', 'Everything later packages build against: headers, registries, save chunk layouts, scene ids, unlock id ranges, stubs.', 'Wave 1')
const w1 = await parallel([
  () => pkg('S1 milestone M1 (tween/beat/camfx + post hooks, shared-file hunks first)', '', 'Wave 1'),
  () => pkg('H2 milestone M0 (GBA harness commands) and H3 milestone M0 (prims.c factoring)', 'Do H2.M0 and H3.M0 only.', 'Wave 1'),
])

phase('Wave 2')
const w2 = await parallel([
  () => pkg('S1 milestones M2-M5, then S2 engine milestone (VM, text, textpack, STRY, verbs, timing, legacy port)', 'Do S1 M2-M5 first and commit, then the S2 engine milestone and commit separately.', 'Wave 2'),
  () => pkg('H1 milestone M1, then H4 milestone M1 (furngen + 60 pieces + room_ref + scene shell), then S3 milestone M1 (authored possessor hook)', 'Commit each milestone separately.', 'Wave 2'),
])

phase('Wave 3')
const w3a = await parallel([
  () => pkg('S2 content (Chapters 1-2, P0, Interlude I, F1, D1)', '', 'Wave 3'),
  () => pkg('S3 content (Chapters 3-4, Interlude III, F2-F4, D2, epilogue and credits; combat/pet/exorcism hooks)', '', 'Wave 3'),
])
const w3b = await parallel([
  () => pkg('H2 (GBA isometric renderer) then H3 (PS1 2.5D renderer)', 'Commit each separately.', 'Wave 3'),
  () => pkg('H1 milestones M2-M3, then H4 milestone M2 (all art, visits, mart, every animation)', 'Commit each separately.', 'Wave 3'),
])
const w3c = await parallel([
  () => pkg('S4 (side missions batch A: 54 missions + the 49 existing quests re-timed)', '', 'Wave 3'),
  () => pkg('S5 batch B (55 missions, quest-chain verification); leave the Feast port for wave 4', '', 'Wave 3'),
])

phase('Wave 4')
const feast = await pkg('S5 Feast port (after S1.M4 stages)', 'Port the existing feast cutscene onto the new story/staging system per SPEC §12 C9/C16 WITHOUT losing any of its art, beats or director-note gags.', 'Wave 4')
const i9 = await pkg('I9 (integration and verification, the last package)', 'Full playthrough tests, docs (README status, docs/design/ROADMAP.md), final gates of §9/§10, GitHub CI green (check ci.yml commands).', 'Wave 4')
return { s0, w1, w2, w3a, w3b, w3c, feast, i9 }
