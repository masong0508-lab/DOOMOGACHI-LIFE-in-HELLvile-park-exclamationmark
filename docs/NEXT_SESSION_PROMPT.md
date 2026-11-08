# Next-session prompt (paste into a new chat together with the repo zip; nothing else is needed)

Project: BOREagotchi LIFE! in HELLvile PARK... (repo root: core/, gba/, ps1/, dc/, docs/, tools/). Owner works from Termux on an Android phone.

## PRIORITY ZERO (before anything else, and again before the last message of every session)
A session that changes the repo is not finished until the owner has BOTH: (1) the patch zip, presented, and (2) the exact
Termux command for it, in a code block, in the final reply. No poems, no status essays, no empty "done" message instead: if the zip
or the command is missing, the work is not delivered. Build the zip first, test that it applies to a pristine copy of the tree,
then write the reply. If a tool fails or the zip cannot be built, say so plainly in the first line and give what is known.
The formulas are in "How to deliver" right below; copy them, do not rewrite them.

## How to deliver (the formula; it lives in the repo so nobody retypes it)
- Never a full archive. Edit a copy of the tree, then build the patch zip:
  `sh tools/mkpatch.sh doomo-<what>-<monthday><letter> "commit message" PRISTINE_TREE EDITED_TREE OUT_DIR`
  (NEW name every patch; the zip holds patch.diff, apply.sh, commit.txt, RESTORE.txt). Put the zip in the outputs folder.
- Tell the owner exactly one command: `sh ~/DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark/tools/dpatch.sh NAME`
  (pulls, applies, commits with commit.txt, pushes). Until the repo has tools/dpatch.sh, give the long form from RESTORE.txt.
- Every patch that changes behaviour also edits the ROADMAP "Refresh" section (move a line from To do to Done) and, if the area has one, its doc.
- Do not hand-write long one-liners; the zip's RESTORE.txt has the copyable commands, so an interrupted session can be rebuilt.

## Read first, in this order
1. This file.  2. `ROADMAP.md`, the section "Refresh 2026-11-06" (done / playable / to do / needs the owner).  3. `docs/STATUS.md`.
4. The doc of the area you touch (`docs/PLAYAS.md`, `docs/COOP_HEROES.md`, `docs/PS1_LIFESIM.md`, `docs/GBA.md`, ...).
A repo zip has no git history: ask which patches are already applied if the roadmap's list of pending patches matters.

## Verify before delivering
`make -C core -j8 test` (expect 861+ unit, 160+ scripts, 19 journeys, 0 failed), `make -C core check-gen`, `make -C core story-check`; after touching
generated data run `make -C core gen` first. State plainly what was NOT built or run (GBA / PS1 / DC have no toolchain in a chat sandbox).
Speed work: `sh tools/perf_profile.sh` ranks core callers by host time; RAM work: `sh tools/ram_ledger.sh`. Host ranks are not console numbers; say so.

## Standing rules
- Focus on the PS1 build unless the owner names GBA, Dreamcast, show or script work (ROADMAP "Current focus").
- Built-in text never contains mature content (fade-to-black fallback only); `mature_adult_ok` and the kid-cast list stay untouched; PG-21 register, adults only.
- Mature pack `hellvile_after_dark.dmpk` lives outside the repo; the "attested" flag is deliberately unset.
- Keep the owner's tone (Walten Files parody, abrupt comedy); disaster scripts and the S-HOUSE definition wait for the owner.
- Small patches that stack cleanly; touch as few files as the job needs so patches do not conflict.
