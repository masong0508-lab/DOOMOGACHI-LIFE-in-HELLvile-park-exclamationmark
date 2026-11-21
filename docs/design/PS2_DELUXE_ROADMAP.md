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
