# DOOMOGACHI PlayStation 2 Deluxe — PS2-0

**Current state: hardware bootstrap, not yet a playable game.** This is
a true PS2 EE/GS/PADMAN homebrew ELF, not a PS1 executable. It does
not yet run the shared simulation or preserve saves. PS2-1 is
the next requirement before any claims of actual content parity.

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
next graphics milestones. Audio is silent and disc-side Talk assets
are not available. PS2-2 now links a real memory-card transport, but
its persistence must still be exercised in PCSX2 and on hardware.
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
generation rollover, but is not a substitute for actual libmc integration
on PCSX2 or hardware. Treat the feature as **experimental until tested**.
