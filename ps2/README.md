# DOOMOGACHI PlayStation 2 Deluxe — PS2-0

**Current state: two experimental PS2 outputs, not a verified game release.**
The original PS2-0 diagnostic ELF remains isolated; the separate PS2-1+
shared-game ELF now links the portable core and has experimental SPU2,
disc Talk, double-bank memory-card writes and a 16-bit GS display path.
None of these constitute a complete PCSX2 playthrough or physical
console acceptance test.

## Build

Install the [ps2dev toolchain](https://github.com/ps2dev/ps2dev)
with PS2SDK and gsKit. Environment:

```sh
export PS2DEV=/usr/local/ps2dev
export PS2SDK="$PS2DEV/ps2sdk"
export GSKIT="$PS2DEV/gsKit"
export PATH="$PATH:$PS2DEV/bin:$PS2DEV/ee/bin:$PS2SDK/bin"
make -C ps2 -j2
# Optional, if CMake is installed:
# cmake -S ps2 -B ps2/build -DCMAKE_TOOLCHAIN_FILE="$PS2DEV/share/ps2dev.cmake"
# cmake --build ps2/build --parallel
```

ELF: `ps2/build/doomogachi_ps2.elf` (launch on PCSX2 or on homebrew-enabled
PS2 with ps2link/ps2client). The PS2 GitHub workflow compiles that ELF in
the upstream `ps2dev/ps2dev` container and uploads it on success.

## Verified functionality to aim for

- GS: NTSC 640x448 interlaced field mode, double-buffered 2D screen.
- PADMAN: DualShock 2 D-pad; Cross=A, Circle=B, Square=X,
  Triangle=Y, L/R shoulders, triggers, stick clicks, Start/Select;
  clean disconnection handling. The map is validated by a native test
  for every button and all-buttons pressed.
- 60 field presentation cadence; **30 logic ticks per 60 field flips**,
  avoiding the accelerated gameplay problem of per-vblank core ticks.
- Developer diagnostic UI (NOT a substitute for the actual game's
  title/Free Play/Story screens). Press D-pad to change a row,
  Cross or Start to toggle details, Circle to hide, Start+Select to exit.
- Explicit build provenance in artifact; no binaries are claimed until CI
  successfully produces the ELF.

## Work to finish before calling this a port

1. Link `core/sources.mk` objects directly; call
   `doomo_init()` once and `doomo_frame(buttons)` at 30Hz.
2. Convert indexed master palette, fonts, procedural bust portraits,
   shading and image transforms to GS sprites (or an upload-managed
   cached software surface). Do not allocate a new portrait pool for PS2.
3. Implement the shared 3D/overworld/room contract with correct map
   geometry, sorting and occlusion, not placeholder vistas.
4. PS2 audio (IOP RPC/mixer), disc-side Talk banks and game assets.
5. Memory-card persistence: PS2-2 now uses two alternating CRC-verified
   files per slot; finish emulator and physical-card power-loss, corruption,
   full-card, removal and re-insertion tests before shipping.
6. PCSX2 boot and full menu/scene walkthrough, then real console tests.
7. Update on-device pad labels to the PS1-like PS2 scheme and add
   PAL/NTSC/widescreen options. Never advertise true 60Hz game logic
   until all scene timing has been measured.

This branch is **stacked on top of** the existing cross-port optimization
PR #5; keep the PS2 PR separate until its prerequisite is merged. It
does not alter the GBA/PS1/Dreamcast builds.

## PS2-1 experimental shared-core game (in progress)

`make -C ps2 game` attempts a separate `doomogachi_ps2_game.elf`.
Unlike the PS2-0 debug ELF, it **compiles and links all portable
`core/src` and `core/gen` sources directly** and invokes the actual
`doomo_init()` and `doomo_frame()` at 30 logic ticks per second.
A fixed 320x240 indexed surface draws the shared font, UI, procedural
portraits and reference room renderer; a fixed-size 512x256 RGBA GS
texture is uploaded at 30Hz and presented at 60 fields/s.
The prototype now adapts the native reference map renderers: live
world cells, authentic 64x64 floor/wall textures, raised faces, lighting,
billboard feet sorting and occlusion, plus the actual first-person DDA
height raycaster with floors, ceilings, skies and step faces. Both
render into the existing 320x240 indexed buffer. No second framebuffer
or per-frame texture allocation is introduced.

**Known incomplete:** geometry is real but still CPU-rendered; GS
native polygons, performance profiling and PCSX2 screenshots are the
next graphics milestones. PS2-3 connects tones and PCM effects to
audsrv/SPU2; PS2-4 reads generated Talk resources from the ISO.
PS2-2 saves still require an emulator power-loss walkthrough.
Do not use this as the released game or treat a successful ELF link
as proof of playability.
The extra experimental ELF is informational CI until PCSX2 testing,
real renderer and save/audio backends pass.

## Fidelity and controller safety

`python3 ps2/tests/check_reference_parity.py` verifies that the PS2
reference overworld and height raycaster remain byte-for-byte equivalent
to the host source **except** for their frame surface include and the
host-only draw counter. This protects against an accidental return to
placeholder floors, ungrounded sprites or inconsistent cell heights.

If PADMAN reports a disconnected controller or an unready pad, the
new EE game loop releases its analog vector as well as its digital
buttons. The core receives the generated left-stick D-pad mask, allowing
its existing analog movement/camera damping to distinguish a stick from
the physical D-pad. Temporary on-screen button labels reuse the PS1
DualShock convention (Cross / Circle / Square / Triangle).

## PS2-2 experimental memory-card persistence

The **game** ELF now uses the PS2SDK `libmc` IOP service and the native
`save_game()` / `load_game()` codec. A formatted PlayStation 2 memory card
in port 0 / slot 0 stores two alternating image files for each core save
slot under `/DOOMOGACHI/` (`STORYA`, `STORYB`, `ISLANDA`,
`ISLANDB`). Each file consists of a 16-byte DMS2 generation/length/CRC
header followed by the **unchanged** checksummed DMOO save image.

A new save writes only the older or invalid copy, flushes and closes the
file, then reopens and verifies the entire content before returning success.
A load validates both copies and chooses the newest fully valid generation.
Incomplete/corrupt newer writes fall back to the previous complete copy;
there is no new save data format for any other console. Card absence or an
unformatted card is a real error, never a successful volatile save.

The save code uses 1 KiB of aligned IOP transfer scratch, no heap and no
extra framebuffer. No auto-format or deletion of unrelated card content.
The game does **not yet** include a PS2 Browser icon, an in-game card-slot
selector, or a verified resume test after process restart/power loss.
The host `save_journal_test` checks CRC, torn-write fallback, bounds and
generation rollover. PS2-6 also tracks a formatted MCMAN card across
successive `mcGetInfo` polls (where `fmt` remains zero) and resets the
state on removal, as covered by the mocked-libmc tests. These are not a
substitute for actual memory-card integration on PCSX2 or hardware. Treat the feature as **experimental until tested**.

## PS2-3/4/5: SPU2, live Talk disc and GS throughput

The **shared-game** target, `make -C ps2 game`, now builds and links
`audio.c`, `synth.c` and `talkdisc.c`. The audio path generates exactly
735 16-bit stereo PCM frames per 30 Hz game tick at 22050 Hz through
PS2SDK audsrv, with four procedural tone channels and three overlapping
SFX voices. PS2-6 adds actual left/center/right tone routing, -64..64
sample stereo panning, semitone pitch adjustment and priority-aware
replacement of overlapping SFX, without expanding the PCM buffer. The audio renderer advances even when SPU2 is behind, so
skipped audio cannot accelerate or delay the simulation. If the IOP
module is unavailable it logs and mutes instead of pretending audio
works. Its audsrv.irx must come from the installed SDK.

The disc path exposes the same `TALK_EN.BIN` compiled by
`make -C core talk`; no new or hand-authored substitute language file
is introduced. It uses PS2 CDVD calls and one 2048-byte aligned sector
cache over the core's existing 256-byte window. The actual Talk scene
is now linked, with controller-accessible quick replies, Ask about
their day and a real social-memory ledger. On an ELF boot without the
language disc it does not advertise Talk. Typed entry is off because
this version has no PS2 keyboard backend.

The GS software surface remains 320x240 indexed and shared with 3D.
Only the *presentation texture* changed: 512x256 RGB5551, 256KiB in
both EE upload scratch and GS VRAM instead of 512KiB RGBA32. This
reduces memory and pixel upload bytes by 50%, but is **not** yet a
native-GS geometry renderer, and visual/alpha parity still needs
PCSX2 screenshots.

For a disc image:

```sh
make -C ps2 disc
# ps2/build/doomogachi_ps2_game.iso
```

This stage requires `xorriso`, Python 3, PS2SDK/gsKit, and the SDK's
`$(PS2SDK)/iop/irx/audsrv.irx`. `SYSTEM.CNF`, the built ELF, the
real Talk bank and audsrv are copied into the ISO root. The audsrv
module is supplied by PS2SDK and has its own upstream licensing;
do not distribute modified binaries without reviewing those terms.

GitHub's PS2 workflow now tests the PS2 host controls, audio mixer,
Talk filename security, save-journal rollback and mocked libmc I/O.
The *shared game ELF* is a required step rather than a warning-only
prototype; an attempted disc build verifies all required root assets.
The real PCSX2 acceptance route is in
`ps2/tests/PCSX2_PLAYTHROUGH.md`. No emulator/BIOS is bundled in CI
and a green compile alone is **not** gameplay verification.

## Reproducible PS2 build and BIOS-free emulator test lab

A developer with Docker on Linux can build the real game ELF and ISO with
one command from the repository root:

```sh
sh ps2/tools/build_container.sh
# Output: ps2/build/doomogachi_ps2_game.iso
```

This uses the official `ps2dev/ps2dev:latest` cross-toolchain image,
not the host's compiler. It installs Make, Python and xorriso in the
temporary image, runs the diagnostic and real-game builds, and packages
the disc-side TALK_EN.BIN and audsrv IRX. No copyrighted PS2 BIOS or
original commercial game files are included.

### First real emulator attempt (no BIOS required)

Play! is a free open-source emulator with its **own high-level BIOS**
implementation. On Ubuntu/Debian Linux with an X11-capable desktop:

```sh
sudo apt-get install flatpak xvfb xauth imagemagick
flatpak remote-add --user --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
flatpak install --user flathub org.purei.Play
dbus-run-session -- xvfb-run -a -s '-screen 0 1280x720x24' \
  python3 ps2/tools/play_biosfree_smoke.py \
  ps2/build/doomogachi_ps2_game.iso --seconds 35
```

The runner preserves `ps2/build/play-smoke/emulator.log`,
`boot-1.png`, `boot-2.png` and `report.json` (including ISO SHA256).
On GitHub Actions the `biosfree-play-smoke` job runs **after** the actual
EE game ELF and ISO have succeeded, retrieves that exact ISO, and
uploads these outputs as `doomogachi-ps2-play-biosfree-capture`.

**Interpretation is strict:** a process still running after 35 seconds
and two PNG files only establish that Play! started and presented
windows. They DO NOT establish that the game rendered, that its controls
work, or that Talk, saves and audio are correct. Inspect the PNGs and
video/logic telemetry before calling anything playable. A Play! HLE
incompatibility is not necessarily a game bug.

For deeper gameplay/PS2 fidelity, use PCSX2 with a BIOS dumped from
your **own PS2**, and run the full acceptance walkthrough in
`ps2/tests/PCSX2_PLAYTHROUGH.md`. No external BIOS is ever fetched by
this project. PCSX2 and hardware acceptance are separate from the
BIOS-free smoke job.

The model's temporary terminal may not have Docker/PS2SDK/Play!
installed and may not have outbound network access. In that situation,
the GitHub Actions job is the repeatable remote environment; the
absence of a completed run must be reported rather than presenting
queued actions as tests that already passed.

## PS2-6 portable follow-up checks

The new CDVD host test executes the actual `ps2/src/talkdisc.c` against
an isolated PS2SDK CD mock. It verifies cached sectors, cross-sector
reads, bounds checking, failed DMA reads, missing media, and resets after
disc initialization failure. The full test is in
`ps2/tests/talkdisc_mock_test.c`. The SPU2 mixer test now exercises
six waveforms, stereo routing, pitched samples, priority stealing and
voice-state bounds. No new audio heap allocations or expanded buffers.

The branch's PS2 build/test jobs may be queued while shared hosted runners
are busy; the workflow cancels superseded PS2 runs for the same branch.
A successful host mixer/CDVD mock test **does not** prove SPU2 output or
optical-disc reads in Play! / PCSX2. Keep the PR draft until the real
EE build and emulator captures pass.

## PS2-7 exact-content GS upload suppression

The renderer now compares the indexed 320x240 output against its existing
512x256 RGB5551 DMA staging texture. If nothing changed, it draws the
resident texture without re-uploading **256 KiB** to GS VRAM. Changed
frames copy exactly the differing pixels and perform the normal full
texture upload; nothing else is allocated. The test
`ps2/tests/indexed_present_test.c` verifies first-upload behavior,
identical-frame suppression, random pixel changes, color parity and
padding guards. A native reproduction passed 134,098 sanitizer-backed
checks, but this is not a PS2 hardware performance measurement. FPS
improvement depends on how many frames are genuinely unchanged. The
portable reference 3D geometry still renders into the indexed buffer.
