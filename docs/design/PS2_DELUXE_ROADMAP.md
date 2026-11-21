# PlayStation 2 Deluxe Edition — implementation roadmap

Status: **proposed; not a built PS2 port**. Owner: cross-platform. Canonical branch: `main`.
Goal: a richer, performant, genuinely playable version of BOREagotchi LIFE! in HELLvile PARK that keeps the existing game's systems, personalities, and story intact while using PS2 capabilities for presentation and simulation.

## Hardware and toolchain baseline

- Target a real PS2 EE + IOP + GS using the community [ps2dev toolchain](https://github.com/ps2dev/ps2dev) and [PS2SDK](https://github.com/ps2dev/ps2sdk), not a PS1 binary in backward compatibility mode.
- Hardware budget: **32 MiB EE main RAM**, **4 MiB GS eDRAM**, and standard **8 MiB PS2 memory card** (do not equate DVD size with memory). Establish measured usable allocations before scene scaling.
- Disc: start with homebrew ELF and emulator smoke tests; add an ISO packaging track when boot flow and asset loading work. No assertion about licensed retail disc booting.
- Shared C99 simulation and data contracts remain authoritative; write a PS2 platform layer, not a second divergent game.

## Talk / life simulation — deluxe targets

- **192 remembered residents** on PS2 as a first proposed target (4 + 192 x 12 = **2308 bytes** for the existing memory entries), with optional expanded trait overrides and additional event journal stored in a separate *versioned* extension, not packed into the current 12-byte record.
- 1000 potential residents remain addressable; only active conversation histories are retained in the fixed LRU. Never confuse number of residents in the world with active-memory table size.
- Active/ready favors must remain pinned; the UI should display a clear 'memory full / commitments preserved' message instead of pretending a new contact was remembered.
- End-to-end invitation pipeline: offer -> contextual acceptance/refusal -> scheduled outing -> navigation/scene load -> activity -> relationship/mood effects -> durable event log. All visible choices must yield a verifiable action or a clear reason they cannot.
- Reactions by personality, mood, affinity, relationship, time and location; third-party interruptions, follow-up chats, rumor provenance, and remembered consequences. Use deterministic seeded scheduling; no per-frame text generation, no large permanent string arrays.
- Separate `TalkMem` hot state from historical events. Persist compact event codes and source/target IDs; generate language-specific replies on demand. Enforce memory/safety/age gates for every port, including scene transitions and save reload.

## Milestones and acceptance criteria

### PS2-0: build and boot

1. Pin toolchain revisions, establish `ps2/` build scripts and `ps2/README.md`.
2. Produce a booting ELF with controller input, screen clearing, framebuffer flip, console log and a frame counter on PCSX2 plus a real-hardware smoke checklist.
3. Add CI for host cross-compilation when a reproducible PS2 toolchain image is available.
**Exit:** repeatable no-crash boot, correct video mode and clean return from first scene.

### PS2-1: shared game functionality

1. Link the existing core simulation, world data, save serialization, platform input, audio and file loading.
2. Build the same menu and scene navigation as the current ports; no blank scene placeholders counted as complete.
3. Implement 8 MiB memory-card saves, CRC, readback and power-loss-safe replacement where supported.
**Exit:** create resident, enter home, Talk, accept a task, save, reboot and complete it. Old data is never overwritten without explicit import.

### PS2-2: Deluxe visual and audio presentation

1. PS2 GS render backend: 3D world geometry with stable perspective, consistent town map-to-world transitions, skeletal/pose animation where practical, dynamic lighting and clear portraits.
2. Bigger rooms, recognizable city vistas and seamless camera continuity in cutscenes; reuse landmarks so events actually occur in the visible world.
3. Richer character expressions, gestures and voice snippets, with streamed music and environmental audio. Precompute and stream from disc; keep GS textures resident only as needed.
4. Performance targets: 60 Hz 2D and 30 drawn FPS for demanding 3D scenes first, then optional 60 FPS profiles where supported. Measure worst 1% frame time and simulation TPS separately.
**Exit:** repeatable scene captures and frame traces for representative worst-case hubs, crowds, Talk and combat; no unbounded load-time spikes.

### PS2-3: Deluxe social systems

1. Implement the larger talk cap, event journal and historical recall with old-save migration tests.
2. Deliver fully playable dates (adults only), outings, activities, promises, group conversations and visible results.
3. Stress-test 1000 resident identities, a full memory table, all active promises pinned, cross-scene travel, save/load and corrupted saves.
**Exit:** scripted interactions produce the same logical outcomes on PS2 and shared host simulation; no silent loss of active commitments.

### PS2-4: content parity, polish, release

1. Complete the existing campaign and free-play loop including endings, levels and access to the hub.
2. Add Deluxe extras **only after parity**: expanded interiors, larger crowds with scheduling, bonus stories, cinematics, audio mixes, and accessibility options.
3. Test controller navigation, 4:3 CRT safe area, widescreen option, memory card full/error handling, PAL/NTSC modes and optional progressive output (only if verified).
4. Package ELF and ISO artifacts, attribution, documented build source SHA, checksums, performance reports and known limitations.
**Exit:** real hardware plus PCSX2 playthrough across all endings and a full free-play day. Verify on hardware before calling the port playable.

## Cross-port compatibility rules

- GBA, PS1 and Dreamcast remain supported and independently benchmarked. Shared semantic behavior; different renderer, memory and text-input profiles.
- Talk caps in current source are GBA 32, PS1 64, Dreamcast 128, host 48. PS2 target 192 requires adding a distinct `DOOMO_PLATFORM_PS2` branch and corresponding save-budget checks when development starts.
- Save payload keeps current 12-byte per-resident format and version. Smaller ports may import a larger PS2 save only with a documented priority order: protect active favors first, then newest meaningful histories, then older casual chats; never silently truncate.
- Add explicit save-size/VMU and memory-card capacity assertions, endian/CRC tests and fuzzed load tests. Do not expand the existing GBA story save sectors or overrun PS1 memory cards to meet a marketing number.
- Memory itself must be useful: measure number of distinct residents that can recall the player, fulfilled/failed promises remembered after reboot, no memory eviction while active, and per-turn CPU/IO overhead. Caps aren't performance results.

## Immediate engineering backlog

- Verify the new per-port Talk cap in real console builds and native tests. Record resulting .bss, serialized chunk size and slot free space.
- Add port-specific over-cap import tests and an explicit result when all memory slots are pinned.
- Add on-device memory-status UI and meaningful callbacks for invitations and promises.
- Create PS2-0 skeleton once toolchain availability and shared-core ABI are confirmed.

## Pre-PS2 optimization and stability gate (November 21, 2026)

The work in [Talk/social PR #5](https://github.com/masong0508-lab/DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark/pull/5)
is a prerequisite to, not the implementation of, PS2-0. **No PS2 source
port, bootable ELF or ISO is claimed by this milestone.**

### Shared kernel and console resource changes

1. The resident-problem queue is unchanged in semantics and save layout;
   two 32-bit presence masks reject the common no-problem case without
   scanning all 24 pending concerns. Removal rebuilds the masks so a
   deleted or replaced entry cannot hide an active request. The GBA
   catch-up traversal now looks up each resident slot only once.
2. The Dreamcast PAL8 texture cache uses a vacant-slot cursor and repairs
   only the evicted entry's local hash collision chain, not the entire
   2,048-entry table. Host tests cover invalid inputs, deliberate hash
   collisions, eviction pressure and frame-parity with the native runner.
3. Dreamcast 3D billboard sort is stable with fixed 1.5 KiB scratch and
   bounded O(n log n) cost for crowds. The 16-sprite small-group path
   stays in-place; equal depth retains original order and the 256-entry
   worst-case sort is covered by unit tests.
4. PS1 and Dreamcast 3D visibility generations are byte-sized with
   wraparound invalidation. On the PS1 this saves **4,096 B** of borrowed
   visible-cell stamps plus **64 B** of texture stamps; the 16-bit
   cell-index list is not touched. On the Dreamcast the analogous arrays
   shrink as well. Both continue to share the same geometry and logical
   simulation; these aren't extra frame buffers.
5. Dreamcast Talk bank prefetch has a minimum 5 ms slot charge even for
   a cached/emulated zero-time GD-ROM sector. The title-to-house
   integration and frame-pacing tests must still pass with the change.

### Must-pass acceptance matrix before PS2-0

- **Native:** compile all core tests and sanitizer builds; same seed
  produces the same simulation checksums, resident relationships, actual
  invitations, news, and story/free-play outcomes.
- **GBA:** ROM and mGBA gameplay/flash-save smoke, 240x160 readability,
  GBA resident-slot and full-capacity safety, no memory-budget regression.
- **PS1:** PSn00bSDK link, overlay-borrow/memory guards, 3D screenshot
  scripts and worst-case primitive budgets; no shortened save scratch,
  unplayable geometry, or hidden 30 Hz logic regressions.
- **Dreamcast:** pinned KallistiOS CDI, host PVR/room/view parity and
  collision tests, Talk streaming host checks, VMU/save, and real
  Flycast boot, Talk, family and Bone-level journeys with recorded
  timing. PR CI is now configured to execute this **before** merging.
- **Release gate:** all required checks must be green at the same final
  commit; inspect logs and artifact sizes. Previous cancelled runs and
  a successful old `main` integration do not prove a newer PR is
  shippable. Benchmark frame time and RAM rather than extrapolating an
  FPS gain from reduced asymptotic cost alone.

### Carry-forward boundary for the eventual PS2 port

Keep the C99 core simulation, deterministic tick cadence, portable
asset/texture identities, story/free-play and Talk transitions, save
serialization/CRC, and error behavior identical. Introduce a separate
platform adapter for PS2 input, display lists/GS, sound, file streams,
card saves and frame pacing. Reuse cross-port scripts as gameplay oracles,
but allow higher-resolution visuals and carefully budgeted deluxe
content **after** PS2-1 gameplay parity. Do not turn a RAM-saving change
in PS1 or a video-RAM cache in Dreamcast into an unverified PS2
allocation assumption.

Status: optimization code committed to the open PR, **final multi-port
verification still pending at the time of this entry**. PS2-0 has not
started.

### Pre-PS2 cross-port optimization: Free Play population scans

The island map, church/hospital menu gates, town records, Father Fuzz's
status speech, Hellvile wall, picnic and rent now visit **occupied physical
resident slots** rather than all 1,000 possible resident IDs. This retains
the full 1,000-person population on PS1/Dreamcast/native, but avoids roughly
half the population iterations and repeated ID-to-slot lookups on the
GBA-500 port (500 resident slots, each resident still keeps a stable ID).
There is no new array, texture, heap allocation, save field or per-frame
rendering pass.

The island crowd snapshot still completes every ~8 frames, and counts
at most 255 per location instead of rolling over to zero at 256 (which
used to hide local-observation actions in busy concerts). The optional
social-vignette picker also visits physical slots and never creates bonds.
A native regression explicitly verifies an isolated high resident ID,
living/ghost/dead status transitions, and the 255+ crowd saturation.
The PS2 port must preserve this slot-index/roster-ID separation.

### Pre-PS2 optimization: resident directory graph membership

The roster's **Couples**, **Singles**, and **Cross-border** filters now
build a 125-byte temporary membership mask from one read-only pass
over the sorted relationship graph, rather than repeatedly searching
up to 2,000 edges for every displayed citizen. The filtered list is
still in the same resident-ID order (or the chosen explicit sort),
and the UI still rebuilds when it is resumed or a filter changes.
In particular, a genuine cross-border friendship is now discoverable
even if it was outside the first 12 contacts in the old scan.

The temporary mask is stack-local, never saved or stored between
frames. A native regression checks that friends, partners and
singles enter/leave the relevant filters when the real graph changes.
The console builds exclude the test-only inspection helper.

### Pre-PS2 portrait response: focused roster loading

The scrollable resident directory used to keep its card blank for **six
stable ticks before asking for a full bust portrait**. Its selected
portrait now requests a background prefetch as soon as the roster
opens or selection changes. A cached real face is drawn on the very
first selected frame; on a miss the existing 16x24 chibi stands in
during sliced composition instead of a blank bust and then upgrades
automatically. With the cursor stationary, the next and previous
citizens are opportunistically warmed. The old admission throttle
remains on all other portrait grids; only the single focused directory
card bypasses it, using `portrait_image_ui` (no synchronous heavy
portrait render on the constrained ports). No save changes or extra
portrait framebuffer/cache blocks. A native scene regression checks
that prefetch starts upon opening the directory, before scrolling.

### Focused resident screen and reaction faces

The one large portrait on a resident's own page now uses the streamed
focus path as well, instead of waiting behind the shared thumbnail
admission budget. The record's actual expression is requested before
transitioning from a roster, apartment, or island link, and independently
on a direct resident-scene entry. Interactions that change expression
queue their new face before starting the corresponding FX/dialogue.
These requests use the existing single background portrait job and
unchanged 18/27KB pool, rather than new portrait textures or allocations.
