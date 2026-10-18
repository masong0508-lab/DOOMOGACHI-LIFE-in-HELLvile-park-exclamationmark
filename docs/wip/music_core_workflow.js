export const meta = {
  name: 'doomogachi-music-core',
  description: 'Build the dynamic chiptune music engine, toolchain, score and Studio/Concert/DJ from docs/design/music/SPEC.md in isolated worktrees, each package verified and pushed',
  phases: [
    { title: 'Engine', detail: 'MU0 contracts + AUD-SPLIT + MU1 M1 + MU1T M1 + MU6 T1/T2/T4/T13' },
    { title: 'Grow', detail: 'MU1T M1.5 + T3 + MU4d banks | MU1 M4' },
    { title: 'Score', detail: 'MU4a/b/c wave-1 cues | MU5b Studio/Concert/DJ' },
  ],
}
const BRANCH = 'claude/nice-allen-gr6czm'
const BASE = `Repo: DOOMOGACHI LIFE (C99 core in core/, GBA port gba/, PS1 port ps1/ via PSn00bSDK at /opt/psn00bsdk; export PSN00BSDK_LIBS=/opt/psn00bsdk/lib/libpsn00b). Read ARCHITECTURE.md and the music implementation SPEC docs/design/music/SPEC.md: its "Critique fixes" section (CF-A..CF-I) is binding and overrides the sections it names. Also respect docs/design/customization/SPEC.md (CS) where the music SPEC defers to it, and docs/design/story/SPEC.md for the story/comic-timing contracts. Decision already taken for CF-I: PS1 scratching uses TAPE (+13.5 KB resident) unless the PS1 ledger cannot hold it after a merge, in which case ship DECK-only and log it in docs/design/INTEGRATION_TODO.md.
GOAL OF THE USER: flowing, dynamic, non-repetitive looping chiptune music that pushes GBA + PS1 sound hardware, with simulated record scratches, while keeping as much RAM free and frame rate as high as possible. Never regress fps or RAM beyond your package's SPEC budget line; prefer the cheaper option whenever the SPEC offers one at equal quality.
You implement ONE work package (named below) within its file ownership (SPEC §21 + CF-E). Other agents (a story/dream-house build and other music packages) work on the same branch at the same time: do not edit files you don't own except the hunks listed for you, keep shared-file hunks small and marked.
GIT: you are in an ISOLATED git worktree. Start with: git fetch origin ${BRANCH} && git checkout -B work-$RANDOM origin/${BRANCH} (if a previous attempt of your package already pushed commits, git log will show them: continue, don't redo). Build dirs are local to your worktree.
VERIFY before every push (all must pass): make -C core test -j8; make -C core test SAN=1 BUILD=build-san -j8; sh core/tools/xcheck.sh; make -C core check-gen; make -C gba -j8 && make -C gba engine-test && make -C gba test; cd ps1 && rm -rf build && cmake --preset ps1 && cmake --build --preset ps1 (clean build: must leave >= 72 KB below the stack top) && cmake --preset ps1-enginetest && cmake --build --preset ps1-enginetest. Report GBA EWRAM/IWRAM (gba/tools/memreport.py) and PS1 free-below-stack numbers before/after. LISTEN: produce the SPEC's listening renders for anything you author and inspect their stats; LOOK at runner screenshots of anything you draw.
PUSH: commit only your package's files (never build dirs, renders or screenshots); message "<PKG>: <summary>" ending with the two lines
Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01RLcGbHBa2kju7ynbaJLHT6
Then: git fetch origin ${BRANCH} && git merge --no-edit origin/${BRANCH} (resolve conflicts; regenerate core/gen files with the repo's generators, never by hand); if the merge brought anything in, re-run the full verification; then git push origin HEAD:${BRANCH}. On a non-fast-forward rejection repeat fetch/merge/verify/push; on network errors retry up to 4 times with backoff. Commit and push each milestone separately so work is never lost to usage limits.
Return a short report: what shipped, files, RAM/CPU/fps numbers before/after, verification results, commit hashes, anything deferred.`
const pkg = (id, extra, phase) => agent(`${BASE}\nYOUR PACKAGE: ${id}. ${extra || ''}`, { label: id, phase, isolation: 'worktree' })

phase('Engine')
const engine = await pkg('MU0 (contracts) + AUD-SPLIT + MU1 milestone M1 (engine, rings, driver, legacy bridge, classic cues) + MU1T milestone M1 (import, mmlc, check-gen, musrender) + MU6 tests T1, T2-native, T4, T13',
  'Order: MU0 (no behaviour change) -> AUD-SPLIT (verbatim move) -> MU1T M1 -> MU1 M1 -> the tests. The game must sound the same as today through the legacy bridge at the end of M1 on all three targets.', 'Engine')

phase('Grow')
const grow = await parallel([
  () => pkg('MU1T milestone M1.5 (musgen + guard + turnarounds, GEN graphs for TITLE, HAHALAND, INVASION, HOME) + MU6 T3 (the velocity-free non-repetition metric of CF-G, with the sensitivity test) + MU4d banks (instruments, comps, grooves, fills, motifs, the 16 vinyl patterns, default transitions, the synth-SFX import)',
    'musgen must reproduce the SPEC fingerprint on gcc and clang. Do not author the MU4a/b/c cues (a later agent does).', 'Grow'),
  () => pkg('MU1 milestone M4 (matrix, stingers, comic bridge, DECK vinyl, GEN bags, chapters, twin, quotes, beds) + MU6 T8 and T9',
    'Stay inside mus_*.c / mus_int.h / music.h / audio_seq.c and your test files; game call-site hookups (MU5a) are NOT yours - a later integration package does them after the story/house build finishes.', 'Grow'),
])

phase('Score')
const score = await parallel([
  () => pkg('MU4a (world) + MU4b (combat) + MU4c (activities) wave-1 cues (SPEC §15 tables), with MU4d stingers/cards and the wave-1 story cues of MU4d',
    'T3 must pass for every cue; produce morph + comic renders and a one-line listening note per wave-1 cue. Use the leitmotifs of §14 and the shipped MINGLE / HEE HEE songs (CF-A).', 'Score'),
  () => pkg('MU5b (Studio LIVE songs, scratch songs, Concert DJ battles: mus_song.c, concert_dj.c, studio_scratch.c, handed-over song_player.c and concert_song.c, and the §17 hunks)',
    'T10 and the T14 DJ path; PS1 M6 line (net -1.4 KB); renders of a LIVE song, a scratch song and a DJ battle.', 'Score'),
])
return { engine, grow, score }
