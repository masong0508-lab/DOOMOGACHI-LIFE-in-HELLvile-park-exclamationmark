export const meta = {
  name: 'wave-runner',
  description: 'Build, independently review and land a list of packages (one git worktree each) on claude/nice-allen-gr6czm',
  whenToUse: 'Each round of a BOREagotchi work stream: pass {wave, stream, context, attribution, evidence, machine, packages:[{id, title, brief, after}]} as args (tools/agents/README.md)',
  phases: [
    { title: 'Build', detail: 'one agent per package in its own worktree' },
    { title: 'Review', detail: 'an independent reviewer per branch' },
    { title: 'Land', detail: 'verify findings, fix, rebase, run the gates, push' },
  ],
}

// ---- arguments (tools/agents/README.md) -------------------------------------------------------------
const A = {"wave": "speed wave 2 (round 2)", "stream": "SPEED", "evidence": "/tmp/claude-0/-home-user-DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark/ea792815-11f0-5f9f-bdfe-5997cdfc4994/scratchpad/evidence", "attribution": "Co-Authored-By: Claude Fable 5.2 <noreply@anthropic.com>\nClaude-Session: https://claude.ai/code/session_01RLcGbHBa2kju7ynbaJLHT6", "machine": "Everything is installed on this machine: the GBA compiler + libmgba harness; PSn00bSDK in /opt/psn00bsdk (export PSN00BSDK_LIBS=/opt/psn00bsdk/lib/libpsn00b, PATH += /opt/psn00bsdk/bin); psxavenc on PATH (configure the PS1 build with -DDOOMO_INTRO_REQUIRED=ON); PCSX-Redux (pinned, /opt/ps1emu/pcsx-redux) + /opt/ps1emu/openbios.bin; KallistiOS (/opt/toolchains/dc/kos/environ.sh); the Flycast core (/opt/toolchains/dc/flycast_libretro.so). Nothing to wait for; never install or rebuild a toolchain yourself. Other agents work on this 4-core, 15 GB machine at the same time: do not touch their worktrees (.claude/worktrees/fut-*), use -j2, one emulator at a time, generous wall-clock timeouts. IMPORTANT: the machine restarted at 2026-11-12 23:57 UTC and killed the agent that was working on this package before you. Its worktree and branch are still there with its commits and uncommitted changes: inspect them first (git log, git status, git diff) and continue from there.", "context": "Owner (2026-11-08): \"speed up everything else please, it needs to run flawless (don't mind about the combat if you can't do anything else about it)\". FPS TARGET: a steady 30 fps on every screen and platform (GBA, PS1, Dreamcast), logic at 30 ticks a second, no frame over 33 ms; combat stays as SP8 left it. SPEED DEFAULTS: pixel-changing steps (a held frame, a short fade over a load, a stand-in or the previous picture for 1-3 ticks) are allowed where exact steps cannot reach 30; each one is listed in the docs with before / after screenshots. The measurements and the package list this wave works from: docs/design/held/SPEED_AUDIT2.md (sections 4-6); the tooling: tools/speed2.py, make -C gba test-speed2 / profile-speed2, ps1/harness/speed2.sh, dc/harness/speed2.sh (docs/GBA.md \"Speed tour 2\" and the \"Speed wave 2: S2-x\" sections, docs/PS1.md \"Speed tour 2\"). Landed: S2-1..S2-8, S2-12, S2-13 and the analog sticks. In flight on this machine (do not duplicate): PS1-RAM (PS1 resident memory; until it lands the PS1 margin is only 65,952 B, so add NO resident PS1 bytes), S2-9 (workshop), S2-10 (saves and loads), and speed audit 3 (a fresh measurement of every scene, docs/design/SPEED_AUDIT3.md when it lands: use its numbers if it has landed when you start).", "packages": [{"id": "S2-11", "title": "House view turn and busy Live layout", "brief": "Speed S2-11, the house leftovers (docs/design/held/SPEED_AUDIT2.md section 6 row S2-11, cause C7 / C5). Two things:\n1. Turning the house view recomposes the room in one tick: 106 ms of work, one 117 ms frame on the GBA. Build the new view over 2-3 ticks in the hidden page while the shown page keeps the old view (the frame-building hook of the island work, I10's plat_build_frame), then flip. Pixel-changing: the old view stays 2 ticks longer (allowed; before / after screenshots). Note: ST1 (landed) lets a right-stick flick turn the view in 90-degree steps with repeat on hold: the sliced turn must work with repeated turns too (a turn asked for during a build is queued or restarts cleanly).\n2. The busy Live layout (four walkers) spends 18% in draw_region and had 27 frames of 50 ms on the GBA; on the PS1 it ran 28.6-28.9 fps (room-test 1). Restore per walker rectangle instead of per row. Exact (same pixels: prove with frame hashes).\nGoal: no frame over 33 ms and a steady 30 fps in every house window of the speed2 scripts and the room tests, on GBA, PS1 and Dreamcast (the Dreamcast house is DC8's plat_room: measure it, do not slow it). Files: core/src/home_draw.c, gba/src/room.c, ps1/src/room.c only if the PS1 gap is real on today's tree. Measure first: S2-1's modal hold and S2-7 changed some of this since the audit."}, {"id": "S2-4b", "title": "Fun Fair: every game and title card at 30", "brief": "Speed S2-4, second part: the Fun Fair. S2-4's first part (docs/GBA.md \"Speed wave 2: S2-4\", ROADMAP \"Speed S2-4, first family\") built the kept playfield (a fair game splits its playfield into still parts baked into the GBA ROM per key by SP7's baker, DOOMO_BACKDROP_MG, and live parts drawn over a whole-picture copy each frame) and applied it to Whack-an-Imp only: GBA 15 -> 21 fps. That is not 30.\nGoal: on the GBA every window of the fair group of the speed2 scripts (gba/harness/scripts/speed2_fair.txt: the seven title cards, Whack, Shooting Gallery, the Derby, Vape Cloud Dodge, Memory Hellfire, Candy Catch, the results) at a steady 30 drawn fps, logic 30, no frame over 33 ms. Audit 2 numbers: Gallery 11 fps (logic 19.4), Whack and Derby 15 (Derby 22 after S2-5), title cards 15, Dodge 20-27; Candy Catch already held 30 (keep it). The PS1 and the Dreamcast hold 30 there today: they must not change.\nHow: profile each window first (make -C gba profile-speed2 SPEED2=fair). Bring Whack from 21 to 30, then apply the kept playfield to the other games; where the playfield copy itself is the cost, move the moving actors (moles, targets, racers, clouds, cards) to OBJ sprites as the PERFORM choir does (docs/GBA.md on G-K1, Option C), or restore only the rectangles under what moved. Exact where possible (frame hashes); pixel-changing steps listed with screenshots.\nCosts to report: Whack's 30 baked pictures took 1.15 MB of ROM (8.00 MB of the 32 MB cartridge now): say what each game adds and prefer restoring rectangles over baking whole pictures where it is as fast. EWRAM / IWRAM: net zero (the scene scratch arena holds per-scene data)."}, {"id": "G2-EX", "title": "Exorcism at a steady 30 on the GBA", "brief": "The exorcism on the GBA runs at about 22.5 drawn fps after SP7 (docs/EXORCISM_SPEED.md: the baseline, the exact wins E1, the structural step E2, the results and \"the gap\"). The owner's target is a steady 30 fps, logic 30, no frame over 33 ms, in every phase of the exorcism (the windows of the speed tour and of ps1/harness/scripts/perf_exorcism_phases.txt's GBA counterpart; make -C gba profile-exorcism).\nGoal: close the gap the doc describes. Start from its own list of what is left and a fresh profile of each phase on today's tree; the PS1 (30.0 fps in perf_exorcism.txt on PCSX-Redux) and the Dreamcast must not get slower. Exact steps first (frame hashes); then pixel-changing steps under SPEED DEFAULTS (held stage parts, effects at half rate, a stand-in for 1-3 ticks), each with before / after screenshots and a line in docs/EXORCISM_SPEED.md. If a phase still cannot hold 30, land what helps and write the measured number, the remaining gap and what it would take. Files: core/src/exorcism*.c, gba/src (the stage / backdrop code SP7 added). EWRAM / IWRAM: net zero."}]}
const REPO = A.repo || '/home/user/DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark'
const BRANCH = A.branch || 'claude/nice-allen-gr6czm'
const EVIDENCE = A.evidence || '/tmp/boreagotchi-evidence'
const wave = A.wave || 'unnamed wave'
const stream = A.stream || ''                 // SPEED, TALK, DC, CREATOR, LEVELS: names the status file
const waveContext = A.context || ''
const packages = A.packages || []
// The two lines every commit message ends with: the lead passes the ones its own session was given.
const ATTRIBUTION = A.attribution || 'Co-Authored-By: Claude <noreply@anthropic.com>'
// What is installed on this machine right now (the lead knows; agents never install toolchains).
const MACHINE = A.machine || `Toolchains and emulators are installed by the lead of this machine with \`sh tools/dev_setup.sh\` (stages apt, psn00bsdk, psxavenc, redux, kos, flycast). Check what is there with ls: /opt/psn00bsdk (PS1 SDK: export PSN00BSDK_LIBS=/opt/psn00bsdk/lib/libpsn00b and put /opt/psn00bsdk/bin on PATH), psxavenc on PATH (the PS1 disc then links the boot FMV: configure with -DDOOMO_INTRO_REQUIRED=ON), /opt/ps1emu/pcsx-redux + /opt/ps1emu/openbios.bin (PS1 harness), /opt/toolchains/dc/kos/environ.sh (Dreamcast build), /opt/toolchains/dc/flycast_libretro.so (Dreamcast harness). If one is missing it may still be installing: do everything that does not need it first, then wait with a bounded poll, for example: timeout 3600 sh -c 'until [ -x /opt/ps1emu/pcsx-redux ] && [ -f /opt/ps1emu/openbios.bin ]; do sleep 30; done'. Never install or build a toolchain or an emulator yourself. If a tool is still missing after an hour of waiting, say so plainly in your report: do not skip its checks silently and never invent numbers.`

const slugOf = (id) => String(id).toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '')

const ENV = (id) => `## Machine and ground rules
- Repo: ${REPO} (the main checkout: do not edit files there and do not switch its branch). The integration branch is \`${BRANCH}\` on origin. CLAUDE.md (already in your context) has the hard rules and the commands.
- Tools on this machine: ${MACHINE}
- The CPU cores are shared with other agents: use -j2, run one emulator at a time, use targeted test targets while iterating and the full gates at the end. Fps numbers come from emulated time (mGBA / PCSX-Redux / Flycast count their own frames), so a busy host slows a run but does not change its numbers.
- Evidence (logs, tables, before/after screenshots) goes to ${EVIDENCE}/${id}/ (create it). Not into the repo.
- Git: never a bare \`git stash\`; never force-push; never push to any branch other than ${BRANCH}; only the LAND step pushes. Every commit message ends with these lines:
  ${ATTRIBUTION.split('\n').join('\n  ')}
- Known and not yours: \`make -C core test SAN=1\` prints a few old UBSan reports in concert_gfx.c / fixmath.c and their tests; they do not fail the run. A new report in code you touched is yours.
- Owner decisions: docs/design/held/OWNER_DECISIONS.md (the latest entry wins). The plan and status of everything: docs/design/HELD_OFF_ROADMAP.md.
- Several streams (speed, Talk, Dreamcast, creator, levels) work on separate machines and land on this one branch at the same time. docs/design/held/PARALLEL_WAVES.md says who owns which files, the shared memory budgets and how to resolve a rebase conflict. Read it before you touch a file outside your stream's area.
- Memory gates: the PS1 EXE built with the boot FMV must end at least 72 KB below the stack top (ps1/tools/elf2exe.py prints the margin at the end of the PS1 build; under 64 KB the build fails). GBA: \`make -C gba sizes\` (EWRAM about 2 KB free, IWRAM nearly full): no new resident EWRAM / IWRAM without saying where it came from.
${waveContext ? '\n## This wave\n' + waveContext + '\n' : ''}`

const GATES = `## Gates (all of them green before a push; while iterating run the ones a step can affect)
1. make -C core -j2 test                          (ends "N passed, 0 failed")
2. make -C core test SAN=1 BUILD=build-san -j2    (if ASan refuses to start: sysctl -w vm.mmap_rnd_bits=28)
3. make -C core check-gen
4. sh core/tools/xcheck.sh                        (no WARNING about a skipped compiler; read the script for the PSn00bSDK include variable it wants)
5. make -C gba -j2 && make -C gba test ; make -C gba sizes ; for speed work also the speed2 groups you touched: make -C gba test-speed2 SPEED2="group ..." SPEED2_GATES=1
6. PS1: cd ps1 && cmake --preset ps1 -DDOOMO_INTRO_REQUIRED=ON && cmake --build --preset ps1 -j2 ; note the "below the stack top" margin ; then the harness (ps1/harness/run.py under xvfb, see docs/PS1.md): game_smoke plus the scripts of the scenes you touched (sh ps1/harness/speed2.sh OUTDIR group ...)
7. make dc-host-test JOBS=2 (needs no KallistiOS). If dc/ or shared drawing / input / save code changed and KallistiOS + Flycast are installed: make -C dc -j2, make -C dc/harness, and the dcrun scripts that cover the change (sh dc/harness/speed2.sh OUTDIR group ...)
Docs with the change: the package's section in the docs of what it touched (docs/GBA.md, docs/PS1.md, docs/DREAMCAST.md, docs/TALK.md, docs/CREATOR.md, docs/LEVELS.md), the RAM ledger rows, a Done line in ROADMAP.md.`

const BUILD_SCHEMA = {
  type: 'object',
  properties: {
    status: { type: 'string', enum: ['ready', 'partial', 'blocked'] },
    branch: { type: 'string' },
    worktree: { type: 'string' },
    head: { type: 'string', description: 'commit sha of the branch head' },
    summary: { type: 'string', description: 'what was changed, in plain sentences' },
    measurements: { type: 'string', description: 'before / after numbers per platform, with the command that produced each' },
    gates: { type: 'string', description: 'each gate: passed / failed / not run (and why)' },
    pixel_changes: { type: 'string', description: 'every visible change, or "none (exact)" with the proof' },
    open_items: { type: 'string', description: 'what is not done, risks, anything the reviewer should look at' },
  },
  required: ['status', 'branch', 'worktree', 'head', 'summary', 'measurements', 'gates', 'pixel_changes', 'open_items'],
}

const REVIEW_SCHEMA = {
  type: 'object',
  properties: {
    verdict: { type: 'string', enum: ['clean', 'minor', 'blocking'] },
    findings: {
      type: 'array',
      items: {
        type: 'object',
        properties: {
          severity: { type: 'string', enum: ['blocking', 'non-blocking'] },
          title: { type: 'string' },
          evidence: { type: 'string', description: 'the command, file:line or measurement that shows it' },
          suggestion: { type: 'string' },
        },
        required: ['severity', 'title', 'evidence'],
      },
    },
    checks_run: { type: 'string', description: 'what you built, ran and measured yourself, with results' },
  },
  required: ['verdict', 'findings', 'checks_run'],
}

const LAND_SCHEMA = {
  type: 'object',
  properties: {
    status: { type: 'string', enum: ['landed', 'landed-partial', 'not-landed'] },
    commits: { type: 'string', description: 'sha + one line for each commit now on origin' },
    final_measurements: { type: 'string', description: 'the key numbers on the rebased, landed tree' },
    ps1_margin_bytes: { type: 'string', description: 'bytes below the stack top on the landed tree (with the boot FMV), or "not built" and why' },
    gates: { type: 'string', description: 'each gate on the rebased tree: passed / failed / not run (and why)' },
    findings_handled: { type: 'string', description: 'each reviewer finding: real or not (the probe), fixed or left' },
    not_done: { type: 'string', description: 'what remains, with measured gaps' },
  },
  required: ['status', 'commits', 'final_measurements', 'ps1_margin_bytes', 'gates', 'findings_handled', 'not_done'],
}

function buildPrompt(p) {
  const slug = slugOf(p.id)
  return `You are the BUILD agent for package ${p.id} (${p.title}) of the "${wave}" wave of BOREagotchi LIFE!.

${ENV(p.id)}
## Your worktree
  git -C ${REPO} fetch origin ${BRANCH}
  git -C ${REPO} worktree add -b fut-${slug} ${REPO}/.claude/worktrees/fut-${slug} origin/${BRANCH}
If that worktree or branch already exists (an earlier run of this package was interrupted): inspect it (git log, git status, git diff), keep what is good and continue from there; do not start over and do not delete someone's commits.
Work only inside that worktree. Commit as you go, in small commits that each pass the fast tests. Do NOT push.

## The package
${p.brief}

## How to work
- Measure BEFORE you change anything (the baseline on this machine, same commands you will use after), then after each step.
- A change claimed "exact" must be proven exact (frame hashes, vhash-seq, runner screenshots byte-identical). A pixel-changing step needs before / after screenshots in the evidence directory and a line in the docs.
- Keep the change as small as the goal allows, in the style of the code around it. Every new piece of logic gets a unit test that fails without it.
- If the goal cannot be fully reached: commit what helps, and report the measured number and the remaining gap. Never describe a number below the target as done.

${GATES}
Run every gate you can before you report. A reviewer and a lander come after you: hand them a branch you believe is ready to land.

## Your report (structured; it is read by the next agents, not by a human)
status "ready" = goals and gates met; "partial" = something useful is committed but not every goal or gate (say exactly what is missing); "blocked" = nothing useful (say why).`
}

function reviewPrompt(p, b) {
  const slug = slugOf(p.id)
  return `You are the independent REVIEWER of package ${p.id} (${p.title}) of the "${wave}" wave of BOREagotchi LIFE!. Another agent built it; you did not. Find what is wrong with it before it lands. Every finding needs evidence you produced yourself: a failing command, a file:line with the input that breaks it, or a measurement.

${ENV(p.id + '-review')}
## What to review
Branch fut-${slug} (the builder's worktree: ${b.worktree}; head ${b.head}). Base: git merge-base origin/${BRANCH} fut-${slug} (fetch origin first). Read the whole diff and the commit messages.
Do not edit the builder's worktree and do not commit to its branch. Build and probe in your own detached copy:
  git -C ${REPO} worktree add --detach ${REPO}/.claude/worktrees/rev-${slug} fut-${slug}
and remove it when you are done (git -C ${REPO} worktree remove --force ${REPO}/.claude/worktrees/rev-${slug}).

## What the package was asked to do
${p.brief}

## What the builder claims (claims to verify, not facts)
${JSON.stringify(b, null, 1)}

## Review for
1. Correctness: bugs, edge cases, undefined behaviour, state that survives a scene change or a reset, save / load compatibility, behaviour on the platforms the builder did not run.
2. The claims: re-run the two or three measurements that matter most for this package (fps windows, the PS1 margin, GBA sizes, a census, a part count) and at least the fast gates (core test, check-gen) plus the gate most likely to break. Check an "exact" claim by comparing frames yourself.
3. The owner's rules: is anything called done below a steady 30 fps? Is every visible change listed with screenshots? Memory gates? The hard content rules of CLAUDE.md for any text or art that changed (read new text and look at new art yourself).
4. Tests: does each new piece of logic have a test that would fail without it?
5. Docs: do the numbers in the docs match what you measured?
Severity: "blocking" = must be fixed before landing (wrong behaviour, a broken gate, a false claim, a broken rule). "non-blocking" = worth fixing or noting.
Report a doubt as a finding with its evidence; the lander verifies each one. verdict: "clean" (no findings), "minor" (only non-blocking), "blocking".

${GATES}`
}

function landPrompt(p, b, r, depNotes) {
  const slug = slugOf(p.id)
  return `You are the LAND agent for package ${p.id} (${p.title}) of the "${wave}" wave of BOREagotchi LIFE!. Branch fut-${slug} (worktree ${b.worktree}) was built by one agent and reviewed by another. You finish it and land it on origin/${BRANCH}.

${ENV(p.id)}
## The package
${p.brief}

## The builder's report
${JSON.stringify(b, null, 1)}

## The reviewer's findings (claims: verify each with your own probe before acting on it)
${r ? JSON.stringify(r, null, 1) : 'The review did not complete. Do the review pass yourself first (correctness, the claims, the owner rules, tests, docs), then continue.'}
${depNotes ? '\n## Landed just before you\n' + depNotes + '\n' : ''}
## Steps
1. Every blocking finding: reproduce it. Real: fix it and add a test that fails without the fix. Not real: keep the probe that shows it. Non-blocking: fix the ones that are cheap and clearly right; list the rest.
2. If the builder reported "partial": finish what can be finished in reasonable time. Never call a number below the target done: land what helps and write the measured number and the gap into the docs.
3. git fetch origin ${BRANCH} ; git rebase origin/${BRANCH}. Other streams land on this branch all the time, so expect conflicts and resolve them by the rules of docs/design/held/PARALLEL_WAVES.md: in docs and ledgers keep both sides' rows (renumber yours); a generated file (core/gen/*, anything check-gen covers) is never merged by hand: take either side and run its generator again; an id or tag that now clashes: renumber yours, never the one already on origin; in source files understand both changes and keep both behaviours. Never revert another stream's commit.
4. Rebuild from clean and run ALL the gates on the rebased tree. Re-measure the package's key numbers on the rebased tree: those are the numbers for the docs and the commit message.
5. PS1 memory on the rebased tree: at least 72 KB below the stack top. If the tree you rebased onto is itself still under 72 KB (the PS1-RAM package has not landed), then your package must not make it smaller at all: compile new PS1 code -Os (DOOMO_PS1_COLD) or put it into an overlay group. Never push a tree whose PS1 disc fails to link.
6. ${stream ? `Update docs/design/held/status/${stream}.md in the same push (its header says what goes in: what landed with sha and key numbers, the PS1 margin and GBA sizes now, what is next, blockers).` : 'If docs/design/held/status/ has a file for this stream, update it in the same push.'}
7. Push: git push origin HEAD:${BRANCH} (fast-forward only; if rejected: fetch, rebase, re-run the gates the incoming commits could affect, push again). Verify with git ls-remote origin ${BRANCH} that origin has your head.
8. Clean up after the push is verified: delete build directories, git -C ${REPO} worktree remove the worktree, delete the local branch fut-${slug}.
If you cannot land (a gate you cannot get green, a conflict that would lose behaviour): do not push. Leave the branch and worktree in place and report exactly what blocks.

${GATES}`
}

const landed = {}

function runPackage(p) {
  const prom = (async () => {
    const b = await agent(buildPrompt(p), { label: `build:${p.id}`, phase: 'Build', schema: BUILD_SCHEMA })
    if (!b) { log(`${p.id}: the build agent returned nothing`); return { id: p.id, status: 'build-missing' } }
    if (b.status === 'blocked') { log(`${p.id}: build blocked: ${b.open_items}`); return { id: p.id, status: 'build-blocked', build: b } }
    const r = await agent(reviewPrompt(p, b), { label: `review:${p.id}`, phase: 'Review', schema: REVIEW_SCHEMA })
    let depNotes = ''
    for (const dep of (p.after || [])) {
      if (!landed[dep]) continue
      const d = await landed[dep]
      const l = d && d.land
      depNotes += `${dep}: ${l ? l.status + '; PS1 margin ' + l.ps1_margin_bytes + '; ' + l.commits : 'did not land (' + (d && d.status) + ')'}\n`
    }
    const l = await agent(landPrompt(p, b, r, depNotes), { label: `land:${p.id}`, phase: 'Land', schema: LAND_SCHEMA })
    log(`${p.id}: ${l ? l.status : 'the land agent returned nothing'}`)
    return { id: p.id, status: l ? l.status : 'land-missing', build: b, review: r, land: l }
  })()
  landed[p.id] = prom
  return prom
}

log(`${wave}: ${packages.length} package(s): ${packages.map(p => p.id).join(', ')}`)
const results = await Promise.all(packages.map(runPackage))
return { wave, stream, results }
