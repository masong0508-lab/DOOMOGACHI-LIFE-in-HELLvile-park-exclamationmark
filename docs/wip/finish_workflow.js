export const meta = {
  name: 'doomogachi-finish-optimize',
  description: 'After the story/house build: RAM pass (G0), customization P0-P6, music backends + hookups + TAPE, then a structural speed + RAM pass and final integration with CI green',
  phases: [
    { title: 'Memory', detail: 'G0: #77-#80 RAM pass on both consoles' },
    { title: 'Custom enablers', detail: 'P0 (freeze window) then P3.M1 (SFX.BIN, PS1 reclaims)' },
    { title: 'Wave 1', detail: 'P5 options | P1.M1 + P2.M1 | MU2 M2 GBA backend + MU3 M3 PS1 backend' },
    { title: 'Wave 2', detail: 'P1-P3 M2 renderers | P4 M1-2 creator v2 + P6 M1 | MU5a hookups + MU1 M5 retire' },
    { title: 'Wave 3', detail: 'deep long tail | music M7 TAPE/rich kernels | music M8 content' },
    { title: 'Wave 4', detail: 'customization systems' },
    { title: 'Optimize', detail: 'GBA structural speed pass | PS1 speed + RAM squeeze' },
    { title: 'Final', detail: 'integration, docs, CI green' },
  ],
}
const BRANCH = 'claude/nice-allen-gr6czm'
const BASE = `Repo: DOOMOGACHI LIFE (C99 core in core/, GBA port gba/, PS1 port ps1/ via PSn00bSDK at /opt/psn00bsdk; export PSN00BSDK_LIBS=/opt/psn00bsdk/lib/libpsn00b). Read ARCHITECTURE.md, docs/design/INTEGRATION_TODO.md and the SPEC(s) your package names: docs/design/customization/SPEC.md (CS; its §9 Critique fixes CF-1..CF-45 are binding), docs/design/music/SPEC.md (its Critique fixes CF-A..CF-I are binding; PS1 scratching = TAPE unless the PS1 ledger cannot hold it, then DECK-only, logged), docs/design/story/SPEC.md (§12 binding) and docs/design/story/GRUMBLEGUTS_AND_DIRECTOR_NOTES.md (honour it; art stays cartoon-modest).
THE USER'S STANDING GOAL: finish everything, and keep as much RAM free and the game as fast as possible on both GBA and PS1. Never regress fps or RAM beyond your package's SPEC line; always take the cheaper option at equal quality; if you find a free win (RAM or speed) inside your files, take it and report it.
You implement ONE work package (named below) within its SPEC file ownership. Other agents work on the same branch at the same time: don't edit files you don't own except your listed hunks; keep shared hunks small.
GIT: you are in an ISOLATED git worktree. Start with: git fetch origin ${BRANCH} && git checkout -B work-$RANDOM origin/${BRANCH}. If a previous attempt of your package already pushed commits, git log shows them: continue, don't redo.
VERIFY before every push (all must pass): make -C core test -j8; make -C core test SAN=1 BUILD=build-san -j8; sh core/tools/xcheck.sh; make -C core check-gen; make -C gba -j8 && make -C gba engine-test && make -C gba test; cd ps1 && rm -rf build && cmake --preset ps1 && cmake --build --preset ps1 (clean build; >= 72 KB below the stack top) && cmake --preset ps1-enginetest && cmake --build --preset ps1-enginetest. Report GBA EWRAM/IWRAM (gba/tools/memreport.py), PS1 free-below-stack and the fps of the scenes you touch (GBA libmgba harness, PS1 PCSX-Redux harness), before/after. LOOK at runner screenshots of anything you draw (240x160 and 320x240); LISTEN to renders of anything you author.
PUSH: commit only your package's files (never build dirs or screenshots); message "<PKG>: <summary>" ending with the two lines
Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01RLcGbHBa2kju7ynbaJLHT6
Then: git fetch origin ${BRANCH} && git merge --no-edit origin/${BRANCH} (resolve conflicts; regenerate core/gen files with the repo's generators, never by hand); if the merge brought anything in, re-run the full verification; then git push origin HEAD:${BRANCH}. On a non-fast-forward rejection repeat; on network errors retry up to 4 times with backoff. Commit and push each milestone separately so work is never lost to usage limits.
Return a short report: what shipped, files, RAM/fps numbers before/after, verification results, commit hashes, anything deferred.`
const pkg = (id, extra, phase) => agent(`${BASE}\nYOUR PACKAGE: ${id}. ${extra || ''}`, { label: id.slice(0, 60), phase, isolation: 'worktree' })

phase('Memory')
const g0 = await pkg('G0 memory pass: tasks #77 (GBA EWRAM >= 24 KB free), #78 (GBA IWRAM: the flash-to-stack move, main stack >= 6 KB), #79 (PS1 >= 128 KB below the stack top, using CS §2.2 levers and the story-group cap from INTEGRATION_TODO: story overlay group 99 KB vs 75 KB cap), #80 (verify + document the RAM tables in docs/)',
  'You own gba.ld, bigbuf.c, gba_plat.h, gba/tools/memreport.py, scene_scratch.c, ps1/tools/ovl_plan.py and the overlay group plan until you finish. Free as much RAM as possible on both consoles without any fps loss (measure). Then record the post-G0 baseline numbers that CS (CF-1) and the music SPEC gates are relative to, in docs/design/INTEGRATION_TODO.md.', 'Memory')

phase('Custom enablers')
const p0 = await pkg('CS P0 (contract, codec, saves, generator): M0a (pure refactor, identical goldens), M0b (sim goldens re-baselined with review), M1',
  'This includes the Phase A rewrite of about 49 files: run it as the SPEC\'s re-runnable script, quickly, and push it as soon as it verifies, since other agents are paused for this window. Capture the goldens on the post-G0 tree first.', 'Custom enablers')
const p3m1 = await pkg('CS P3 milestone M1 (PS1 RAM moves: the SFX.BIN move and the other reclaims)', 'Also add the music hunks P3 owes in ps1/src/spu.c (weak plat_tone/plat_sample, SPU_SFX_END, spu_sfx_addr()).', 'Custom enablers')

phase('Wave 1')
const w1 = await parallel([
  () => pkg('CS P5 milestones M1-M2 (options, sinks, controls, remapping: wave-1 options of CS §8)', '', 'Wave 1'),
  () => pkg('CS P1 milestone M1 + P2 milestone M1 (palfx, the interpreter, tones, 28 list appends)', '', 'Wave 1'),
  () => pkg('Music MU2 milestone M2 (GBA backend: PSG v2, the lean mixer, packed stereo, tiers, governor, VBlank driver, legacy off on GBA), then MU3 milestone M3 (PS1 backend: the SpuRead check first, SPU map, banks, bookshelf, root counter, prefill, reverb, locking, legacy off on PS1)',
    'GBA: no scene may lose fps (the governor trials of CF-B); IWRAM +86 B max. PS1: the boot-screen chunked load.', 'Wave 1'),
])

phase('Wave 2')
const w2 = await parallel([
  () => pkg('CS P1, P2 and P3 milestone M2 (14 core painters, 47 deep fields, impostor and model, third person)', 'Creator >= 20 fps GBA / >= 28 PS1; island40 >= its G0 figure - 0.5 fps.', 'Wave 2'),
  () => pkg('CS P4 milestones M1-M2 (creator v2 UI) + P6 milestone M1 (premade variety, derivation tables, seed search)', '', 'Wave 2'),
  () => pkg('Music MU5a (all game call-site hookups of music SPEC §9.3 / §21.3 rows 9-14 and CF-31 rows 26-39, every scene/map/cutscene cue of CF-D) + MU1 milestone M5 (retire the old interpreter)', 'T8, T11, T14; L_PUNCH ticks equal on all three targets.', 'Wave 2'),
])

phase('Wave 3')
const w3 = await parallel([
  () => pkg('CS wave 3: the deep long tail (P1-P3 M3, P5 M3, P6 M2-M3) in the SPEC\'s batch order', 'Per batch: distinctness, pixel visits, art review via contact sheets.', 'Wave 3'),
  () => pkg('Music M7: MU1 M7 TAPE feed + MU2 M7 (rich kernels, TAPE/echo, crackle synth, S/S+, timer-1) + MU3 M7 (TAPE + encoder, crackle, PMON options)', 'The record scratches must sound like real vinyl on both consoles; no fps loss.', 'Wave 3'),
  () => pkg('Music M8: the wave-2 content of SPEC §15 (every remaining cue), with T3 passing for all', '', 'Wave 3'),
])

phase('Wave 4')
const w4 = await pkg('CS wave 4: systems (P2 M4, P4 M4, P6 M3: Look Book, situational outfits, Look Password, ageing, mobility, prosthetics, hybrids, +24 species, PS1 3D turntable, photo mode)', 'Kid/teen safety gates (test_romance_age_gate) must hold.', 'Wave 4')

phase('Optimize')
const opt = await parallel([
  () => pkg('GBA structural speed pass: every scene to its best possible fps (target 30, never below 20): per-scene static-layer caching, dirty rectangles, ROM-baked backgrounds, IWRAM hot loops, DMA, fewer redraws. Known slow: the free-play island hub (~7.5 fps), dialogue, pet, exorcism and concert (~100 ms/frame); measure every scene first with the harness and fix the worst first',
    'Pixels must stay identical unless a change is invisible at 240x160. Report a before/after fps table for all scenes. Then free any further EWRAM/IWRAM you can.', 'Optimize'),
  () => pkg('PS1 speed + RAM squeeze: every scene at 30 fps in PCSX-Redux (known: concert/dialogue ~22, island hub ~24), GPU primitive batching, texpage/CLUT caching, GTE use, fewer overlay reloads, faster CD loads; then free as much resident RAM as possible (overlay plan, const data to overlays/CD, Huffman text)',
    'Report a before/after fps table for all scenes and the RAM ledger.', 'Optimize'),
])

phase('Final')
const fin = await pkg('Final integration (I9 for this workflow)', 'Full scripted playthroughs (story mode + free play) on native, GBA and PS1; every scene reachable with no stubs; docs (README status, docs/design/ROADMAP.md, RAM and fps tables); the gates of all three SPECs; then push and confirm GitHub CI is green on the final commit (read .github/workflows/ci.yml; fix anything red).', 'Final')
return { g0, p0, p3m1, w1, w2, w3, w4, opt, fin }
