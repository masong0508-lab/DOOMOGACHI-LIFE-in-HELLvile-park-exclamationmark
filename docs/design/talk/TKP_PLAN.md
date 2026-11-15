<!-- docs/design/talk/TKP_PLAN.md: Talk on the PS1, measured and planned (package TKP0-MEASURE, 2026-11-15).
Read-and-measure package: no resident PS1 byte changed. The numbers come from the tools listed in section 8.
Re-measured on the rebased tree (PS1 numbers, SPU map; the tree is in the package evidence), after CR10 landed the part store in SPU RAM. Written against origin/claude/nice-allen-gr6czm. Supersedes the PS1 estimates of
TALK_DESIGN.md 6.3, 6.4 and 8.1 where they differ (the design is from before the grammar and the talk_scene existed). -->

# TKP: Talk on the PS1 (measured, and the build plan)

## 0. Verdict

**Talk code fits the PS1. Talk data does not fit SPU RAM as it stands: that needs an owner decision (D1).**
Three facts changed since the design was written, and a fourth (CR10) changed the data plan.

1. **The code is 92 KB, not 24 KB, and it is not a house part.** It is a whole scene (`SCENE_TALK`, the
   owner's `talk_scene.c`), so it becomes **one new overlay group** (`talk`, 92 KB, slot not grown). The
   design's two 12 KB parts (`talk`, `talkgen`) are gone. Resident cost with the group alone: **15,616 B
   measured** (margin 110,560 -> 94,944 B); after the clean-ups of TKP2 about **2 to 4 KB**. That is inside
   the 12 KB share and leaves 8 KB or more for traits and quests. The margin stays far above 72 KB.
2. **The data is 565 KB for the PS1, not 312 KB.** `TALK_en.BIN` is 909,052 B; the PS1 loads 577,786 B
   against the design's 319,488 B budget (181 %). The design's levers (top-6 association, attribute tail)
   are worth almost nothing now. **Lossless re-encoding gets to about 437 KB** (word-token string codec
   -90 KB, packed rows -50 KB).
3. **SPU RAM has no room for 437 KB, because CR10 took it.** The design gave Talk 0x30000-0x7DFFF. Since CR10
   (`partstore.c`) the part scripts and icons (PARTS.BIN, 143,360 B) sit at 0x50000-0x7DFFF, and
   `SPU_SFX_END = SPU_PARTS_BASE`. What is left for Talk is **0x0C000-0x4FFFF = 278,528 B** (effects move
   to a 40 KB region; they use 27.6 KB), or 331,776 B if Talk may also use the unused tail after PARTS.BIN
   and the reverb area (two ranges; CR10 must agree). 437 KB is 32 to 57 % too big for both. It would fit
   only if the part store left SPU RAM. **Main RAM cannot hold it either** (the margin above the 72 KB gate
   is about 36 KB). The CR10 conflict is settled in favour of the part store; Talk must live in what is left.
4. **Generation costs about 40 times the design's estimate** and reads the database about 4 times
   more than its budget. Measured on the host: 661,000 x86 instructions a line (census), 74 KB of reads an
   exchange (48 % of the instructions are the safety lint's probes). A block cache and one lint pass per
   line bring the reads under budget; **the CPU cost is the risk for the 30 fps gate** and needs a sliced
   job (TKP4).

**Owner / coordinator decisions needed:** D1 (what Talk on the PS1 is, given 278,528 to 331,776 B of SPU RAM:
a reduced "PS1 tier" of the content, or drop Talk on the PS1; needed before TKP3), D2 (the 1.5 s load gate
passes for any tier that fits; only relevant if D1 is answered with a bigger store), D3 (the content budget).
Nothing is needed for the resident share.

Packages: TKP1 to TKP6, one day each (section 7).

## 1. What was measured and how

Base: the rebased tree after CR10, PS1-RAM and LV3 (PS1 numbers re-measured on it; the host numbers of section 5 were taken earlier and do not depend on the PS1 map), clean worktree, PS1 built with `cmake --preset ps1 -DDOOMO_INTRO_REQUIRED=ON`.
Commands and logs are in section 8.

| Baseline (nothing linked) | Value |
|---|---|
| PS1 margin below the stack top, with the boot FMV | **110,560 B** (gate 73,728 B; share of TALK 12,288 B; it was 132,608 B before CR10 and the other PS1 work that landed since) |
| Overlay slot | 129,872 B at 0x801c5010 (ends 0x801e4b60) |
| Biggest groups (slot demand, planner estimate) | story 129,972 B, combat 129,956, home 129,196 |
| SPU RAM | effects end at 0x8BE0 (`spu: 35 of 35 effects (all), top 08be0`, measured before CR10 landed; TKP3 re-checks), **27.6 KB used**; `spu.c` header map: effects 0x02000-0x4FFFF, **PARTS.BIN (CR10) 0x50000-0x7DFFF, 143,360 B used of 188,416 B** |
| Overlay load rate | freeplay 103 KB in 314 ms, home 166 KB in 502 ms: **3.0 ms per KB** (2x drive) |
| `game_smoke`, `home.txt` | PASS; `home_steady` 30.00, `home_plain` 30.17 |

## 2. (a) Resident bytes if Talk were linked

`ps1/tools/talk_size.sh` compiles `core/src/talk*.c` with the PS1 flags (`-Os`, R3000) and sums the pieces
(`size`, then a `ld -r --gc-sections` from the roots `SCENE_TALK` and `doomo_save_ext_talk`).

| File | text + rodata | data | bss |
|---|---:|---:|---:|
| `talk_gen.c` (generator) | 37,008 | 52 | 0 |
| `talk_dm.c` (drama manager) | 25,440 | 4 | 0 |
| `talk_scene.c` (the screen) | 20,332 | 164 | 3,957 |
| `talk_lex.c` (typed line) | 19,932 | 0 | 0 |
| `talk_traits.c` | 11,988 | 4 | 164 |
| `talk_safe.c` (gate and lint) | 7,048 | 0 | 4 |
| `talk_parse.c` (typed line) | 6,708 | 0 | 0 |
| `talk_outcome.c` | 6,168 | 20 | 912 |
| `talk_db.c` (reader) | 4,340 | 8 | 296 |
| `talk_plan.c` | 3,360 | 12 | 0 |
| `talk_save.c` (the chunk) | 3,284 | 4 | 292 |
| `talk_act.c` | 932 | 0 | 0 |
| **Total (12 files)** | **146,540** | 268 | 5,625 |

(`story_talk.c`, the old ambient chatter, is already linked: 9.8 KB, not new.) After `--gc-sections`: 95,468 B
of code, 14,220 B of rodata, 256 B data, 5,625 B bss = **115,569 B without the typed line**.

**The typed line is not needed on the PS1.** `talk_lex.c` and `talk_parse.c` (26 KB) read the PARSE bank,
which the PS1 never loads. `talk.h` already has `TALK_NO_UNK` and `talk_scene.c` has `TALK_TYPING` for the
GBA (`DOOMO_PLATFORM_GBA`); the PS1 takes the same two switches (a one-line change each).

**Real links** (experiment trees in the scratch area, nothing committed; same compiler, linker and planner):

| Experiment | What | Margin below the stack top | Resident delta |
|---|---|---:|---:|
| baseline | Talk not linked | 110,560 B | 0 |
| A | `DOOMO_HAS_TALK`, new overlay group `talk` (lex, parse in it) | 91,040 B | **+19,520 B** |
| B | A plus the typed line off (`TALK_NO_UNK`, no `TALK_TYPING`): group 92 KB | 94,944 B | **+15,616 B** |

Both link and the slot does not grow (talk's slot demand is 125,908 B; the slot is 129,872 B). The experiments need one more edit than the flags: `talk_draw` calls `ui_bar_still`, which exists only with `UI_PANELS`, so the experiment uses `ui_bar` there (TKP2 does the same). In A
the planner puts these resident (`OVL_WHY_OBJ=talk_scene` etc.), and each has a fix:

| Resident today (B) | Why | Fix (TKP2) | Saves |
|---|---|---|---:|
| `.bss` of the scene, outcome ledger, reader: `pinfo` 2,412, `T` 1,520, `led` 912, `db` 296 | bss always stays resident | state into the overlay slot's scene scratch (`SCRATCH["talk"]`, `plat_scene_scratch()`, as the island and the workshop do) | ~5.1 KB |
| `row_text` and its strings, `act_text`, `person_lookup`, `home_talk_enter` (3,064 B of the scene) | callbacks whose address is taken (`ui_list`) and a call from `fp_resident:cb_manage` | list them in `ovl_plan.py` LOCAL_CALLBACKS; `home_talk_enter` becomes a 12-byte stub that pushes the scene | ~3.0 KB |
| `talk_db` 2,536 B | called by `home_talk_enter` (open) and by `talk_save:talk_mem_begin` | the stub opens nothing; `talk_mem_begin/end` (session only) move into the group | ~2.5 KB |
| `talk_save` + `talk_traits` ~5 KB | the save chunk read and write (`talk_read` -> `talk_trait_save_read` -> normalize ...) | keep the chunk as an opaque 320 B blob in resident bss and parse it when Talk opens (save version unchanged) | ~4.5 KB |
| `sim_nudge_ok` 320 B and bits | TM2e actions | stays | - |

Result: **about 2 to 4 KB resident** (raw-chunk hooks 0.5 KB, blob 0.3 KB, stub, `sim_nudge`, alignment),
the design's own figure (3 KB). **Gate for TKP2: margin >= 72 KB and Talk resident delta <= 6 KB**
(half the 12 KB share; traits and quests are the other half).

**Group growth.** The group is 92 KB plus a scratch area of about 29 KB (state 5.1 KB, safety pin 6.4 KB,
block cache 16 KB + tags 1 KB) = 121 KB of the 129,872 B slot: **about 6 to 8 KB of room**. TM3 (traits drive
replies) and TQ (quests) add code to these files: more than ~6 KB between them moves the slot, and every KB of
slot is a KB of resident RAM. TKP2 puts the headroom in `status/TALK.md` and the lead watches it.

## 3. (b) The PS1 subset of `TALK_en.BIN` (new banks)

`python3 core/tools/talkc.py --stats --lang en`: 4,009 concepts, 9,062 alternatives in 112 grammar files
(the design counted 6,000).

| Bank | Stored | PS1 loads |
|---|---:|:-:|
| STRINGS (9,030 template strings; 453,297 B raw) | 277,808 | yes |
| GRAMMAR (3,238 rules, 9,062 alts, 6,913 guard terms) | 122,714 | yes |
| ASSOC (top-8, 4,096 rows x 16 B) | 64,176 | yes |
| FORMS | 53,334 | yes |
| CONCEPT | 32,088 | yes |
| ATTR | 18,896 | yes |
| SAFETY | 6,445 | yes |
| CATNAMES, CATS, BEATS | 1,409 + 536 + 380 | yes |
| PARSE | 280,030 | no (DC / native) |
| EDGES | 50,532 | no (DC / native) |
| **Whole file** | **909,052** | |
| **PS1 subset** | **577,786** | **181 % of the design's 319,488 B budget** |

History of the same figure: 209,070 (K-10), 312,185 (T5B-ASK), 345,634 (T5B-TOPIC alone), 396,555 (rebased),
577,786 now (the line bank grew to 9,062 alternatives).

## 4. (c) Which levers are needed

Rule from the gate "same text as native for the same seed": **a lever that removes data changes the text, so
it must apply to every platform's bin. A lossless lever changes only the encoding**, and one reader decodes
both (a bank-kind flag; native, GBA and DC keep their current banks and the parity test decodes the same
text). The plan therefore uses lossless levers only, and treats content cuts as a last resort.

| Lever (design 6.3) | Verdict | Measured / estimated effect |
|---|---|---:|
| Shorter alternative encoding | **needed.** STRINGS is Huffman over bytes (277,808 B, near the order-0 byte entropy 268,785 B). A word-token canonical Huffman (tokens seen twice or more, rare words spelled) is **188,241 B** with the dictionary and the 2 B string offsets (`strings_experiment.txt`); phrase merging and an implicit space should give ~170 KB. Reference points: lzma 135,296 B, zlib 162,294 B (not random access) | **-89,600 B** (to -108,000) |
| Packed rows (new) | **needed.** ASSOC rows hold 7.1 entries of 8 on average (21,254 entries over 2,974 rows): variable rows 42,508 B + index. ATTR has at most 4 attributes: 12-bit packing 14,172 B. GRAMMAR alts drop `term_first` (it is a running sum): 8 -> 6 B (-18,124 B), rules likewise (-6,476 B). FORMS front-coded (estimate) | **-13,700; -4,700; -24,600; -8,000** |
| ASSOC to top-6, ATTR tail | **not needed, not useful:** top-6 saves 9,600 B over the variable rows but changes text; the tail beyond 6 is empty | 0 |
| SPU RAM split | **needed, and not enough (D1).** The design's region 0x30000-0x7DFFF is gone: CR10's PARTS.BIN is at 0x50000-0x7DFFF. Effects capped at 0x0C000 leave **0x0C000-0x4FFFF = 278,528 B**; with the unused tail after PARTS.BIN (0x73000-0x7FFFF, 53,248 B) **331,776 B** in two ranges. The 437 KB subset exceeds both | - |
| Streamed, non-resident file | **rejected as the main path.** A line makes ~93 random reads of 64 B (section 5); each CD seek is 100 to 250 ms. Per-act clusters need ~3 seeks (~0.45 s) an exchange. Keep as a fallback for the cold half of STRINGS only | - |
| Overlay of the data | not applicable (it is data; the code is already an overlay group) | - |
| Dropping DC-only content | already done: PARSE and EDGES are DC / native only (`TBF_DC_ONLY`); nothing in the bin is GBA-only | 0 |
| Dropping content | **now the only lever left.** To fit 278,528 B (a 272,000 B bin) the lossless 437,186 B needs **-165,000 B (38 %)**; to fit 331,776 B (a 325,000 B bin) **-112,000 B (26 %)**. The other way out, moving PARTS.BIN out of SPU RAM (CR10), would free it but is not a Talk decision | needs D1 |

**Subset after the lossless levers: 577,786 - 89,600 - 13,700 - 4,700 - 24,600 - 8,000 = 437,186 B** (range
425,000 to 445,000 B). That is still 158,658 B over the 278,528 B that SPU RAM can give Talk. The levers that
change no text are used up; what is left is the content itself (a "PS1 tier", D1). The non-string banks alone
are about 242 KB after the levers (GRAMMAR 92, ASSOC 51, FORMS 45, CONCEPT 32, ATTR 14, SAFETY 6, rest 2), so
a 272,000 B bin keeps only ~30 KB of strings: a tier cannot just drop strings, it has to drop concepts and
their rows together (about 2,500 of 4,009 concepts kept).

## 5. (d) Load time and (e) generation

**(d) Load at the CD rate.** Planning rate 3.05 ms per KB (the measured overlay rate; the 2x drive's nominal
300 KB/s is 3.33). First Talk of a session: read the subset in 32 KB steps into `plat_save_scratch()`, DMA to
SPU RAM, behind the "pull up a chair" iris (each step blocks ~110 ms, no drawing). The bank stays in SPU RAM
for the session.

| Subset | Read at 3.05 ms/KB | Plus first seek (0.1 to 0.3 s) | Gate 1.5 s |
|---|---:|---:|:-:|
| 577,786 B (today) | 1.72 s | 1.8 to 2.0 s | fails |
| 437,186 B (lossless levers; does not fit SPU RAM) | 1.30 s | 1.4 to 1.6 s | **borderline** |
| ~420,000 B (phrase tokens) | 1.25 s | 1.35 to 1.55 s | borderline |
| 325,000 B (tier for 331,776 B of SPU RAM) | 0.99 s | 1.1 to 1.3 s | passes |
| 272,000 B (tier for 278,528 B) | 0.83 s | 0.9 to 1.1 s | passes |
| 312,000 B (the design's budget) | 0.93 s | 1.0 to 1.2 s | passes |

Besides the data, **entering Talk costs two overlay loads**: the `talk` group (92 KB, ~0.28 s) and, on
leaving, the house group again (166 KB, 0.50 s measured). About 0.8 s in total, behind the same iris and the
existing load fade; the 1.5 s gate is for the data. Order the banks so the PS1 reads **one contiguous range**
(DC-only PARSE and EDGES last; today they sit in the middle, which would cost a second seek): TKP1.

**Reads while talking** (`core/tools/talk_reads.sh`, 5,000 census contexts, 8,573 lines; counted inside
`talk_gen_exchange`): 17,792,436 reads, 371,988,428 B = **2,075 reads and 43.4 KB a line, 74 KB an exchange**
(the design's budget is 16 KB a turn): GRAMMAR 632 reads / 5.1 KB a line, SAFETY 1,403 reads / 37.4 KB a
line (the lint re-reads the register list for every filler), the rest small. Simulated read-through block
caches (direct-mapped would be a little worse than the LRU simulated here):

| Cache | Bytes read a line | Per exchange (1.7 lines) |
|---|---:|---:|
| none | 43.4 KB | 74 KB |
| 64 x 512 B, LRU, session | 20.8 KB | 35 KB |
| 128 x 256 B, session | 8.8 KB | 15 KB |
| **256 x 64 B (16 KB), session** | **5.8 KB** | **9.9 KB** |
| 512 x 64 B (32 KB), session | 2.7 KB | 4.6 KB |
| 512 x 64 B, emptied every exchange | 8.3 KB | 14 KB |
| 4,096 x 64 B (256 KB, the most the tool simulates; still 21,399 misses over 8,573 lines) | 0.2 KB | 0.3 KB |

Plan: pin SAFETY (6.4 KB) and use a **16 KB, 64 B-line cache** in the scene scratch (5.8 KB a line, 93 SpuReads
a line). The SpuRead speed is unknown (design: 2 ms per KB, to be measured in TKP3): 5.8 KB would be 12 ms a
line at that figure, so TKP3 reports it before TKP4 sets the slice size. A 32 KB cache does not fit the slot
with the code (section 2).

**(e) Generation time.** Not measured on an R3000 (the code has no data path on the PS1 yet; TKP3 gives it
one). Estimate from counted instructions:

* Host, `talk_gen_exchange` over 400 census contexts (valgrind callgrind): 437.4 M instructions for 662
  lines = **661,000 instructions a line**; the lint's `talk_safe_lint_probe` is 211.9 M of them (**48 %**), the
  database reader 167.2 M (38 %, replaced by the cache on the PS1).
* Host time per `talk_dm_step` over random 2- and 3-resident sessions (`talk_steps.sh`, 6,255 steps):
  mean 34.0 us, p95 93.4, p99 124.8, max 753.5 us. Per line in the census 105 to 128 us.
* R3000 (33.87 MHz, 4 KB I-cache, 64-bit hashes in `talk_safe` cost 3 to 4 instructions each), taking MIPS
  instructions = 1.2 x x86 and a CPI of 1.5 to 2.5 for 92 KB of code: **a typical step 12 to 30 ms, p95
  33 to 80 ms, worst 90 to 260 ms**. Cross-checks: the Dreamcast measured work per tick 2.5 to 7.5 ms, worst
  11.9 ms (TD3) at about 6 times the PS1's speed; the GBA's Talk tick is about 48 ms of work (TUI-TALK).
* So one `talk_dm_step` often does not fit a 33.3 ms frame, and the worst ones take several frames. **The
  30 fps gate needs the job sliced inside a step** (TKP4), not only between steps (the DC's `talk_scene_slice`
  is step granularity).

**30 fps plan.** The screen is 2D (`ui_*`), no room view: its draw is small. Per tick: draw + input first
(<= 8 ms), then generation until a deadline of 18 ms from the frame start (the PS1 has `timer.c`
root counters), then present. The generator gets a resumable state machine with at most one of these between
deadline checks: pick the rule and alternative (`pick`), expand one part (`make_part`, up to 7 a line), join,
**one** lint of the joined line (replacing the per-filler probes: -48 % of the instructions), the gate. While
the exchange is generated the screen shows the "..." emote and keeps animating; the bubbles appear when
the lines are ready (the drama manager's 300 to 600 ms "thinking" beat). Budget: an exchange ready in
0.6 s or less. Fallback if a part still exceeds a frame: allow it only while the screen is a still "..." (a
held frame, not a stutter), and report the count of frames over 33 ms in the speed2 table.

## 6. Decisions

* **D1, owner (needed before TKP3): what Talk is on the PS1.** The measured facts: SPU RAM can hold at most
  278,528 B for Talk (331,776 B if CR10 lets Talk use the tail after PARTS.BIN and the reverb area); the
  lossless bin is 437,186 B. Choose one:
  * **(a) PS1 tier (recommended if Talk is wanted on the PS1).** `talkc.py --ps1-tier BYTES` writes a reduced
    `TALK_en.PS1`: all of SAFETY and the whole kid-cast and `mature_adult_ok` data, every act, and about 2,500 of
    the 4,009 concepts with their alternatives (272,000 B), or about 3,000 (325,000 B). Which concepts stay is a
    content call (the owner, or the TALK lead by usage: the census counts which concepts and alternatives get
    used). The cost is that a PS1 conversation draws from a smaller store, so the text for the same seed is no
    longer the same as native's full bin. The gate becomes "same text as native **run on the same PS1 bin**"
    (the host reads `TALK_en.PS1` in place, TKP1b). The effects region cap moves from 0x4FFFF to **0x0C000**
    (`spu.c` `SPU_SFX_END`, a constant; 13 KB of growth left over today's 27.6 KB).
  * **(b) Take space from the part store.** 437 KB fits only if PARTS.BIN (143,360 B, CR10: 19,639 B of scripts and
    121,577 B of packed icons) left SPU RAM: 0x0C000-0x7FFFF is 475,136 B. That undoes CR10 on the PS1 and is
    **not recommended**; the part store was settled first.
  * **(c) Drop Talk on the PS1** (`PLATF_TALK` off; core, tools, GBA, DC unaffected). Reason, measured: the lossless
    data is 437 KB against 278,528 to 331,776 B of SPU RAM, and main RAM has about 36 KB to spare over the gate.
    Streaming the cold part from the CD is not an option: a line makes about 93 random reads of 64 B even
    with a 16 KB cache, and a CD seek is 100 to 250 ms.
* **D2, owner (only if D1 grows the store beyond (a)): the 1.5 s first-load gate.** Any tier that fits SPU RAM
  loads in 0.9 to 1.3 s and passes. The 437 KB bin would be 1.4 to 1.6 s.
* **D3, owner / TALK lead: a content budget.** The PS1 bin is capped at the tier size (272,000 B or 325,000 B):
  `talk-check` fails over it (`TK11`, TKP1b). New alternatives after TM3 and TQ then cost ~21 B each (lossless
  format) and must fit under the cap, or the tier is chosen again.
* Not needed: any increase of the 12 KB resident share (expected 2 to 4 KB; gate 6 KB).

## 7. Packages (one day each)

Common gates for every package: `make -C core -j2 test` ("N passed, 0 failed"), `make -C core check-gen`,
`sh core/tools/xcheck.sh`, `make -C gba -j2 && make -C gba test && make -C gba sizes` (net zero EWRAM and IWRAM),
`make dc-host-test`; from TKP2 on the PS1 build with the margin and `game_smoke`. Final gates of the stream:
**margin >= 72 KB (73,728 B) with the Talk resident delta within the 12 KB share; first load <= 1.5 s; same text as
native for the same seed (on the same bin: the full bin if the owner answers D1 with a bigger store, the PS1 tier if D1(a));
every sound effect still plays; steady 30 fps.** TKP1b and TKP3 wait for D1; TKP1, TKP2 and TKP4 do not.

### TKP1: the PS1 data format (host only, lossless)  [M]

Files: `core/tools/talkc*.py`, `core/include/doomo/talk.h`, `core/src/talk_db.c`, `core/src/talk_gen.c`
(row decoders), tests. New bank kinds (flag `TBK_WORD`, `TBK_PACKED`): word-token string codec, variable
ASSOC rows, 12-bit ATTR, 6 B GRAMMAR alts and 6 B rules, front-coded FORMS. `talkc.py --bin --ps1` writes
`TALK_en.PS1` (DC-only banks omitted, PS1 banks contiguous, one 64-byte-aligned range; **about 437 KB, which
does not fit SPU RAM by itself**: TKP1b cuts it). Native, GBA and DC keep `TALK_en.BIN` unchanged.
Acceptance: (1) `test_talk_db`: every string, row, attribute and form of `TALK_en.PS1` decodes to the same bytes
as `TALK_en.BIN` (all 9,030 strings, all concept rows); (2) the census with the `.PS1` bin gives the same
hashes (Surface / Skeleton / Meaning distinct counts) as with the `.BIN` on 20,000 contexts; (3) subset
`<=` 445,000 B and printed by `--stats`; (4) a seeded bad bank (wrong token) is refused by the CRC;
(5) GBA sizes unchanged (the GBA keeps its bank).
Gate: same text for the same seed on both bins.

### TKP1b: the PS1 content tier (host only; needs D1(a))  [M]

Files: `core/tools/talkc*.py` (`--ps1-tier BYTES`), `core/data/talk/` (a tier list: concepts kept, ranked by the
census usage counts), `core/Makefile` (`talk-check` rule `TK11`: the PS1 bin is at or under the tier size),
tests. The tool drops whole concepts (their concept row, ASSOC row, attributes, alternatives and strings) from
the lowest use up, keeps every act, all of SAFETY, the kid-cast list and `mature_adult_ok` untouched, and fixes
the references that point at a dropped concept (no dangling id may reach the reader).
Acceptance: (1) `TALK_en.PS1` <= 272,000 B (or 325,000 B if D1 grants the tail), printed by `--stats`; (2) the
census on the PS1 bin: no empty line, no unresolved reference, the Q8 budgets of TM2f unchanged in kind
(same limits, measured again); (3) the safety lint and the kid-room census give the same refusals as on the full
bin (hashes of the refusal lists equal; no line differs in a kid room except by which concept it names);
(4) the host reads the tier bin in place and its text for a seed is what the PS1 path will print (TKP3
compares); (5) native, GBA and DC bins are byte-identical to before.

### TKP2: the `talk` overlay group and the resident clean-up  [M]

Files: `ps1/CMakeLists.txt` (`DOOMO_HAS_TALK=1`), `ps1/tools/ovl_plan.py` (GROUPS `talk`, SCRATCH, LOCAL_CALLBACKS),
`core/include/doomo/talk.h` and `talk_scene.c` (`TALK_NO_UNK` and no `TALK_TYPING` for the PS1;
`ui_bar_still` exists only with `UI_PANELS`, so the engage bar uses `ui_bar` there), `talk_save.c` (opaque
chunk blob), `talk_scene.c` / `talk_outcome.c` / `talk_db.c` (state in the scene scratch), `home_talk_enter`
stub. The scene opens without data and says "Talk is not ready" until TKP3.
Acceptance: `ovl: talk` loads; `talk` group <= 100 KB and slot demand <= 131,076 B (slot not grown);
**margin >= 72 KB and Talk resident delta <= 6 KB** (target 2 to 4 KB) with `ps1/tools/elf2exe.py`'s number in the
commit; `grep "talk_" ps1/build/doomogachi.ldmap` shows only the listed resident sections;
the old save loads and the TALK chunk round-trips (blob in, same bytes out, `test_talk_save` through the PS1 path
on the host); `game_smoke` PASS; every screenshot of the other scenes byte-identical.

### TKP3: the data path: `talkdisc.c`, the SPU split, the block cache  [M]

Files: `ps1/src/talkdisc.c` (new: find `TALK_EN.PS1`, 32 KB steps through `plat_save_scratch`, DMA into SPU RAM
from 0x0C000 up to 0x4FFFF, 278,528 B at most, `plat_talk_load` / `plat_talk_read` with the 16 KB direct-mapped 64 B cache in the scene scratch
and SAFETY pinned), `ps1/src/spu.c` (`SPU_SFX_END`), `ps1/CMakeLists.txt` and `iso.xml` (the file),
`hcmd.c` (`talk-load`, `talk-crc`).
Acceptance: first load <= 1.5 s logged by the build (`talk: loaded N B in T ms`); SPU read-back CRC equals the
file's; `audio_selftest` passes and the boot log says `spu: 35 of 35 effects (all)` with `top` below the cap;
`sfx` script plays every effect; reads <= 16 KB a turn and the SpuRead rate (ms per KB) recorded in the doc;
`TALK_en.PS1` read in place on the host equals the PS1 path's text; `spu.c` `SPU_SFX_END` is the new 0x0C000 and
the part store (CR10) is untouched: `partstore` self-test and the creator part script harness still pass.
Gate: margin unchanged from TKP2 within the talkdisc code (<= 2 KB).

### TKP4: the generation job (sliced, one lint a line)  [M]

Files: `core/src/talk_gen.c`, `talk_dm.c`, `talk_safe.c`, `talk_scene.c` (the tick), `ps1/src/timer.c` use.
A resumable generator (states: pick, parts, join, lint, gate) that checks a deadline from the frame's start
(18 ms); one lint of the joined line instead of a probe per filler (same refusals: the lint tests must pass
unchanged, plus a new test that fails when a probe-only word slips through the joined lint); the host exposes the
same job so the native runner can slice too.
Acceptance: **same text as native for the same seed** (a new `talk-hash` script command prints an FNV of the
lines; native, DC and PS1 scripts `expect-talk-hash` equal; the census hash over 20,000 contexts equal before and
after the lint change); no frame over 50 ms and at most 2 over 33.3 ms in a 20-exchange script
(`speed2_talk` on PS1); an exchange ready in <= 0.6 s; the instruction count per line recorded with
`talk_steps.sh` (host) before and after.

### TKP5: the Talk screen on the PS1: input, draw, 30 fps  [M]

Files: `core/src/talk_scene.c` (small layout changes only; SPEED owns the draw code), `ps1/harness/scripts/`
(`perf_talk.txt`, `home_talk_*` copies), `docs/PS1.md`.
Acceptance: steady 30.0 fps in the windows main page, acts page, topic list, answer playing, tense room (3
residents), `fps` lines in the log; the pad reaches every control (press budgets of the native journeys:
quick reply 1, turn 4); screenshots next to the native ones; every sound effect plays on entering and leaving.

### TKP6: close-out  [S]

Files: `docs/TALK.md` (PS1 section), `docs/PS1.md` (SPU map, RAM ledger rows), `ROADMAP.md`,
`status/TALK.md`, this file (measured numbers replace the estimates).
Acceptance: the five final gates above measured on one tree and written down; the running total of Talk
resident bytes on the PS1 in `status/TALK.md` (the share of 12 KB); the Talk overlay group's headroom stated.

Dependencies: TKP1 and TKP2 can run in parallel (host data format / PS1 link); TKP1b needs TKP1 and D1(a); TKP3 needs TKP1b and TKP2; TKP4 needs TKP3;
TKP5 needs TKP4; TKP6 last. TQ and TM3 land code in the same files: rebase onto them and re-run the group size
check each time.

## 8. Risks

1. **30 fps is the one real risk.** The generator may need more than the slice trick to hold a frame (a
   single `gen_line` is 0.66 M instructions on average, ~2 M for a topic-heavy line, and the host's worst step is
   22 times its mean). Mitigation in order: one lint a line, the cache, finer states. Last resort: a still "..."
   frame while a long part runs (counted and reported, not hidden).
2. **Group headroom** (10 KB) against TM3 and TQ.
3. **Load time** is borderline; the plan measures before asking.
4. **SPU RAM is full** after CR10: Talk gets 278,528 B (D1). Any growth of the effects (cap 0x0C000) or a bigger
   PARTS.BIN takes from the same pool; TKP3 records the free bytes.
5. **Text parity with lossy cuts.** None are planned; any later cut must go into every platform's bank.
6. The design's HM_TALK room view (zoom 2 framing, `plat_room_project`, billboard squash, TP3) is **not part of
   this plan**: the shipped Talk is the 2D screen. If the owner wants the room view later it is a separate package
   with its own house-part budget.

**Kill criterion** (the owner said "scratch it"): drop Talk on the PS1 if D1 is answered with (c) or no tier is accepted, or if after TKP4 the
resident share, the load time or the fps gate still fails; write up the measured number and tell the coordinator
first. `PLATF_TALK` off leaves everything else untouched.

## 9. How to re-measure

```sh
# resident cost if linked (needs a PS1 build for compile_commands.json)
export PSN00BSDK_LIBS=/opt/psn00bsdk/lib/libpsn00b PATH=$PATH:/opt/psn00bsdk/bin
cd ps1 && cmake --preset ps1 -DDOOMO_INTRO_REQUIRED=ON && cmake --build --preset ps1 -j2 && cd ..
sh ps1/tools/talk_size.sh /tmp/talk_size          # per file, after gc-sections
# bank sizes and the PS1 subset
python3 core/tools/talkc.py --stats --lang en
# reads a line by bank, and the read-through cache simulation
TALK_CACHE=256:64 sh core/tools/talk_reads.sh 5000
# time of one drama-manager step on the host
sh core/tools/talk_steps.sh 6 300
# instruction counts
valgrind --tool=callgrind core/build/talk_census --grid core/data/talk/census_en.tsv --contexts 400 --seed 1
# real link with Talk and the group (experiment, see section 2): ps1/CMakeLists.txt gets
#   target_compile_definitions(${_t} PRIVATE DOOMO_HAS_TALK=1)
# and ovl_plan.py GROUPS gets  ("talk", r"talk_(?!save)\w+$")  before "gallery"
# OVL_WHY_OBJ=talk_scene shows why a section stays resident
```
