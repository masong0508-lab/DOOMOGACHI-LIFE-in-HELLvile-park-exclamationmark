# DOOMOGACHI PlayStation 2 Deluxe — PS2-0

The shared-game ISO boots the portable core in Play! and reaches Story using
the Qt virtual controller. The independent diagnostic ELFs remain available
for renderer and startup checks. The ISO also includes disc-backed Talk,
SPU2 audio and journaled memory-card saves.

The controller gate compares the same unmodified ISO with and without input.
It checks the ISO checksum, capture times and input schedule before comparing
the frames. The full PCSX2 and physical-console walkthroughs remain open.
[Play! controller evidence](https://github.com/masong0508-lab/DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark/actions/runs/44965048660)

## Build

Install the [ps2dev toolchain](https://github.com/ps2dev/ps2dev)
with PS2SDK and gsKit. Environment:

```sh
export PS2DEV=/usr/local/ps2dev
export PS2SDK="$PS2DEV/ps2sdk"
export GSKIT="$PS2DEV/gsKit"
export PATH="$PATH:$PS2DEV/bin:$PS2DEV/ee/bin:$PS2SDK/bin"
make -C ps2 -j2
# Gameplay ISO with the generated Talk bank and PS2SDK sound modules.
make -C ps2 -j2 disc
# Progressive-scan image for displays that support PS2 480p.
make -C ps2 -j2 disc PS2_VIDEO=480p
# Optional, if CMake is installed:
# cmake -S ps2 -B ps2/build -DCMAKE_TOOLCHAIN_FILE="$PS2DEV/share/ps2dev.cmake"
# cmake --build ps2/build --parallel
```

ELF: `ps2/build/doomogachi_ps2.elf` (launch on PCSX2 or on homebrew-enabled
PS2 with ps2link/ps2client). The PS2 GitHub workflow compiles that ELF in
the upstream `ps2dev/ps2dev` container and uploads it on success.

The standard `doomogachi_ps2_game.iso` uses the 640×448 interlaced
presentation. `doomogachi_ps2_game_480p.iso` uses 640×480 progressive
scan and exactly doubles each 320×240 source pixel in both directions
with nearest sampling. It applies to the title, interface, overworld,
portraits, rooms, and combat through the same GS presentation path.
Both images use the same memory-card format and 30-tick game loop.
The video mode changes the GS screen buffers, while the compact
texture transfer remains 150 KiB per changed frame.

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
the physical D-pad. Digital-only or short PADMAN replies supply no axes.
On a stable digital reply from a capable DualShock, the game requests
analog mode asynchronously. Temporary on-screen button labels reuse the PS1
DualShock convention (Cross / Circle / Square / Triangle).

## PS2-2 memory-card persistence

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
Play! verified two Free Play saves, reload after an emulator restart,
and rollback after corrupting the newest generation in a copy of the
test card. The host `save_journal_test` checks CRC, torn-write fallback,
bounds and generation rollover. PS2-6 tracks a formatted card across
successive MCMAN `mcGetInfo` polls and resets the state on removal.
Story persistence, card removal during a write, and console playback
remain part of the release walkthrough.

The transport uses the PS2SDK's IOP open flags for libmc readback and writes.
The host mock enforces those flags and rejects writes through read-only
handles. It checks both save slots, truncated writes, corrupt banks and RPC
reinitialization.

## PS2-3/4/5: SPU2, live Talk disc and GS throughput

The **shared-game** target, `make -C ps2 game`, now builds and links
`audio.c`, `synth.c` and `talkdisc.c`. The audio path generates exactly
735 16-bit stereo PCM frames per 30 Hz game tick at 22050 Hz through
PS2SDK audsrv, with four procedural tone channels and three overlapping
SFX voices. PS2-6 adds actual left/center/right tone routing, -64..64
sample stereo panning, semitone pitch adjustment and priority-aware
replacement of overlapping SFX, without expanding the PCM buffer. The audio renderer advances even when SPU2 is behind, so
skipped audio cannot accelerate or delay the simulation. Where the ROM
provides LIBSD, the game uses it. BIOS-free Play! can use `FREESD.IRX`
from the disc. Load the sound driver before calling audsrv. If audsrv
cannot initialize, the game logs the failure and continues muted.

The disc path exposes the same `TALK_EN.BIN` compiled by
`make -C core talk`; no new or hand-authored substitute language file
is introduced. It reads the ISO file through the SDK filesystem with a
2048-byte aligned block cache behind the core's 256-byte window.
The raw CDVD path could find the directory entry but failed its sector
read in Play!. The actual Talk scene
is now linked, with controller-accessible quick replies, Ask about
their day and a real social-memory ledger. On an ELF boot without the
language disc it does not advertise Talk. Typed entry is off because
this version has no PS2 keyboard backend.

The 320x240 indexed software surface is shared with 3D. Its RGB5551
presentation texture uses 150 KiB of EE staging and 160 KiB of
page-aligned GS VRAM. The prior 512x256 RGB5551 path used 256 KiB in
each. gsKit rounds the sampler dimensions independently of the upload.
PCSX2 and hardware remain part of the visual acceptance walkthrough.

For a disc image:

```sh
make -C ps2 disc
# ps2/build/doomogachi_ps2_game.iso
```

This stage requires `xorriso`, Python 3, PS2SDK/gsKit, and the SDK's
`audsrv.irx` and `freesd.irx`. `SYSTEM.CNF`, the built ELF, the
Talk bank and both sound modules go into the ISO root. The original
sound module sources are in [PS2SDK](https://github.com/ps2dev/ps2sdk/tree/master/iop/sound).

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
the disc-side `TALK_EN.BIN`, `AUDSRV.IRX` and `FREESD.IRX`. The
image uses the game's own assets and open-source SDK modules.

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

The Talk disc host test executes `ps2/src/talkdisc.c` against an isolated
file at the SDK's disc path. It verifies cached blocks, cross-block
reads, bounds checking, short reads, invalid headers, missing media, and
reinitialization after a failed open. The full test is in
`ps2/tests/talkdisc_mock_test.c`. The SPU2 mixer test now exercises
six waveforms, stereo routing, pitched samples, priority stealing and
voice-state bounds. No new audio heap allocations or expanded buffers.

The branch's PS2 build/test jobs may be queued while shared hosted runners
are busy; the workflow cancels superseded PS2 runs for the same branch.
Use emulator or hardware playback to validate SPU2 output and disc reads
in addition to the host tests. Play! verified the English Talk bank with
a resident greeting and the interactive quick-reply menu. Keep the PR
draft through the PCSX2 acceptance walkthrough.

## PS2-7 exact-content GS upload suppression

The renderer compares the indexed frame against the persistent
320x240 RGB5551 staging image. An unchanged frame reuses the resident
texture. A changed frame uploads 150 KiB, down from 256 KiB. At 30
changed frames per second this saves up to 3,180 KiB/s of GIF texture
payload. The compact staging image saves 106 KiB in EE memory and the
page-aligned texture saves 96 KiB in GS VRAM.

The host test checks packed pixel order, random mutations, identical
frames, and buffer guards. Play! produced pixel-identical viewport
captures for all seven phases of the separate GS texture animation
using the old and new layouts. Measure frame times on the target
console during the PCSX2 and hardware acceptance walkthrough.

## PS2-9 compact disc images and low-storage CI artifacts

Three outputs are available from the **same, real game build**:

| Format | How to use | Is it natively playable? |
|---|---|---|
| ISO | Game image for PS2 homebrew loaders, disc tools, Play! and PCSX2 | Yes, subject to successful game/loader testing |
| CHD | Losslessly compressed DVD image for modern PCSX2 and other CHD-enabled emulators | Yes on compatible emulators; not a generic PS2 hardware/OPL format |
| ISO.ZST | High-ratio Zstandard download/backup of the ISO | No. Decompress to ISO first |

The native disc recipe includes `SYSTEM.CNF`, `DOOMOG.ELF`,
`TALK_EN.BIN`, `AUDSRV.IRX` and `FREESD.IRX`. It uses xorriso's -no-pad option by
default so the ISO does not carry the historical 300 KiB trailing pad.
For a physical disc project where extra padding is desirable, use
PS2_ISO_PAD=1. No gameplay resources, Talk words or music are discarded.

### Local build

On Linux, install MAME's chdman (typically mame-tools), zstd and
xorriso alongside the existing PS2SDK/gsKit toolchain:

```sh
make -C ps2 compact
# Native ISO: ps2/build/doomogachi_ps2_game.iso
# Emulator CHD: ps2/build/compact/doomogachi_ps2_game.chd
# Archive: ps2/build/compact/doomogachi_ps2_game.iso.zst
# Verification: ps2/build/compact/manifest.json
#               ps2/build/compact/SHA256SUMS.txt
#               ps2/build/compact/SIZE_REPORT.md
```

The Zstandard level defaults to 15 and can be tuned with
PS2_ZSTD_LEVEL=19 for stronger (slower) compression, or a lower level
for faster turnaround. To compress a prebuilt ISO without rebuilding
the PS2 application:

```sh
python3 ps2/tools/compact_iso.py \
  --iso ps2/build/doomogachi_ps2_game.iso \
  --out-dir ps2/build/compact --zstd-level 15
```

To restore the original bytes:

```sh
chdman extractdvd -i ps2/build/compact/doomogachi_ps2_game.chd -o restored.iso
zstd -d -f -o restored.iso ps2/build/compact/doomogachi_ps2_game.iso.zst
```

Use different output paths if restoring both formats concurrently.
The packager validates the ISO9660 primary volume descriptor and all
required boot/data files before conversion. It then runs chdman verify,
extracts the CHD, decompresses the .iso.zst archive, and compares each
entire restored ISO's length and SHA-256 to the baseline. Any mismatch
stops artifact publication. The manifest reports actual bytes saved;
CHD might save less than Zstandard depending on the generated build.

### GitHub Actions workflow and retention

The existing PS2 EE workflow has a **compact-images** job that
downloads the game ISO produced by the exact same workflow, rather
than rebuilding a different game. It uploads compact files, SHA256SUMS
and a size comparison for **14 days**. The temporary uncompressed ISO
artifact remains available for **3 days** so that the Play! boot job
can still retrieve it. GitHub's upload step disables redundant ZIP
recompression of CHD and Zstandard files.

The independent **PS2 compact-image packaging tests** workflow runs
real chdman and zstd against a synthetic ISO fixture, checking the
complete round trip, required disc files and failure paths. This
host-side test can run even if the EE compiler is unavailable. It does
not claim that the synthetic ELF is executable or that the real game
has passed emulator testing. The release gate is still an actual
cross-compiled game ISO and successful Play!/PCSX2 walkthrough.


## PS2-10: disc ELF size and actual Play! visual evidence

The unstripped game ELF contains DWARF debugging information. Previously
all of that metadata was copied to the ISO, inflating downloads without
adding gameplay content. Now the `disc` target builds a second ELF using
PS2DEV's MIPS strip tool with `--strip-debug`. The original unstripped
`build/doomogachi_ps2_game.elf` stays available for debugging; only the
smaller `build/doomogachi_ps2_disc.elf` becomes `DOOMOG.ELF` in the
ISO. `ps2/tools/check_disc_elf.py` compares the original and compact
ELFs' **entry points and every byte in every loadable program segment**.
Any change to actual loaded code/data, or a stripping run that fails to
reduce size, halts packaging rather than shipping an unverified image.
This is an on-disc storage reduction, not a reduction in running RAM.

A review of the earlier Play! screenshots (run 44950195103) revealed
that although the emulator loaded DOOMOG.ELF and its process survived
35 seconds, the entire game viewport was black at both 12 and 35
seconds. The previous capture was only a process-liveness pass, **not a
rendering success**. The optional EE-console heartbeat was also absent
from the emulator log (possibly not forwarded by Play!), so gameplay
execution remains unverified. The new capture gate computes visible
pixel coverage inside the game viewport using ImageMagick, excluding
the emulator's menu/status bars, and rejects two black frames. Its
failure still uploads both screenshots, the log and JSON evidence.

You can repeat the visual check against captures using:

```sh
python3 ps2/tools/check_play_frames.py \\
  ps2/build/play-smoke/boot-1.png ps2/build/play-smoke/boot-2.png
```

Only a successful newly built disc and a visible gameplay screenshot
may clear the visual boot gate; an actual playable release also requires
controller, Talk, sound, memory-card persistence and scene traversal.


## PS2-11 graphics-first startup and PCSX2 visual verification

The PS2 shared game's startup now creates the GS video context and
presents a four-color loading/progress screen *before* initiating SIF RPC,
memory card, disc, audio, and controller services. Graphics setup and
gameplay use 16-bit CT16 GS display buffers to preserve VRAM alongside
the existing RGB5551 texture. The progress bar advances between optional
IOP init stages, making a frozen dependency easier to locate in a real
emulator. This is not a fake main-menu replacement and the real shared
core still supplies all gameplay and scenes. The host
`ps2/tests/test_boot_order.py` guards the startup ordering.

The previous BIOS-free Play! builds opened but displayed black
screens in both the game and hardware diagnostics. Its hosted runner
now records `glxinfo -B` and forces llvmpipe/Mesa software OpenGL in
the Flatpak emulator, to distinguish software-GL/Xvfb rendering trouble
from a bug in the EE game. The capture gate still **fails** on a black
viewport, preserving its images and error log.

For the more accurate PS2 compatibility target, on **Linux** with
an official PCSX2 installation, a locally configured BIOS dumped from
your own PS2, ImageMagick and X11/Xvfb:

```sh
python3 ps2/tools/pcsx2_visual_check.py \
  ps2/build/doomogachi_ps2_game.iso --seconds 45
```

The runner captures two frames, records the source ISO SHA-256,
the console log, image brightness diagnostics and a JSON report under
`ps2/build/pcsx2-smoke/`. It does not bundle/download any BIOS.
A non-black screenshot only verifies that pixels appeared, and can
still be the BIOS/settings UI. **It is not permission to declare the game
playable:** finish the actual PCSX2 playthrough checklist for controls,
Talk, SPU2 audio, saves surviving an emulator restart, menus, and 3D.
