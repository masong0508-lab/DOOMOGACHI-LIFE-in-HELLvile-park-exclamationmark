# dc/ - BOREagotchi LIFE! in HELLvile PARK... for the Dreamcast

A self-booting Dreamcast disc running the shared C99 core on KallistiOS.
The port is being built in stages (the DC packages of the project plan);
full documentation: **[docs/DREAMCAST.md](../docs/DREAMCAST.md)**.

The whole core is linked, boots, runs its 30 Hz loop and changes scenes,
with the harness mailbox and logging in place (DC2). All of the 2D is drawn
on the PVR (DC3: the 320x240 screen at 2x, matching the native runner
pixel for pixel on the host test). The controller, analog stick and
keyboard work, and saves go to the VMU (DC4). The music, the voice babble
and the sound effects play through a software mixer and one AICA stream
(DC5). The overworld is real 3D on the PVR (DC7: terrain, blocks,
billboards with depth, the native runner's light and tints), drawn inside
the 2D layer's painter's order through the 3D batches of `src/dc3d.h`.
The dream house is the PVR's too (DC8: `plat_room` draws exactly what the
native reference `room_ref.c` draws, with the room's static shell as one
cached texture, each piece one sprite in room_ref's order, real
translucent ghosts, a pulsing selection glow and the zoom; about 3 ms a
tick instead of DC3's 28-50 ms through the 2D calls). DC6 (the
first-person view) replaces the last placeholder in `src/plat_stub.c`.

## Build

```sh
dc/tools/setup_kos.sh --install-deps   # once: KallistiOS + sh-elf gcc 15 (about 40 min)
make dc                                # from the repo root: ELF + CDI
make -C dc                             # or: just the ELF (+ map, size report)
make -C dc disc                        # build/doomogachi.cdi (+ 1ST_READ.BIN, IP.BIN)
make -C dc test                        # ELF + CDI + check-elf + the 2D host test (+ Flycast: boot + ui-compare)
make -C dc ui-compare                  # DC3: the six UI scripts on Flycast against the native runner
make -C dc vmu-test                    # DC4: VMU saves, full / no VMU, first boot, stick + keyboard on Flycast
make -C dc audio-test                  # DC5: title, invaded town, combat with sound on Flycast against runner --wav
make -C dc ow-test                     # DC7: the overworld scripts on Flycast against the host's software PVR
make -C dc room-test                   # DC8: room_ref's golden frames and the house scripts on Flycast
make dc-host-test                      # the 2D layer, the 3D batches and overworld, the house, the VMU file system and controls, the sound mixer on the host (no KOS needed)
make -C dc clean
```

The Makefile sources `<prefix>/kos/environ.sh` itself when the KOS
environment is not set (prefix: `DC_PREFIX`, else `/opt/toolchains/dc`,
else `~/dc`). Every core source (`core/sources.mk`) and `dc/src/*.c` is
compiled with `kos-cc -std=gnu99 -O2 -Wall -Wextra -Werror` and the objects
are linked directly (never through an archive) with `--gc-sections`.

| output | what |
|---|---|
| `build/doomogachi.elf` | the program (load address 0x8c010000), with `.map` |
| `build/doomogachi.cdi` | self-booting disc image (audio/data, MSINFO 11702) |
| `build/1ST_READ.BIN`, `build/IP.BIN` | the scrambled binary and the bootstrap, for other disc formats |
| `build/doomogachi-nodebug.elf` | the ELF without debug sections, for Flycast (it cannot open an ELF over 16 MiB) |
| `build/shots/ui/` | `ui-compare`: Dreamcast and native shots, diff pictures, `summary.txt` |
| `build/shots/ow/` | `ow-test`: per script the Flycast, native and host shots, logs, `compare.txt`; `summary.txt` |
| `build/shots/house/` | `room-test`: `parity/` (the golden frames from Flycast and room_ref, diffs), per house script the Flycast, host and native shots, logs, `compare.txt`; `summary.txt` |

## Run

```sh
flycast dc/build/doomogachi.cdi                    # Flycast (no BIOS needed: HLE)
dc-tool-ip -t <ip> -x dc/build/doomogachi.elf      # real console, Broadband Adapter + dcload-ip
dc-tool-ser -t /dev/ttyUSB0 -x dc/build/doomogachi.elf  # real console, coder's cable + dcload-serial
```

A CD-R burned from the CDI boots on consoles that still read MIL-CDs (the
earlier production runs); an optical drive emulator takes it on any model.
Not yet tried on hardware.

## Files

| file | what |
|---|---|
| `src/main.c` | `KOS_INIT_FLAGS`, video (640x480, VGA or interlaced by cable), PVR init, the frame loop and its 30 Hz pacing |
| `src/pad.c`, `src/padmap.c`, `src/dcinput.h` | the controls (DC4): controller, analog stick, triggers, keyboard; `plat_features`; the mapping is portable |
| `src/vmusave.c`, `src/vmusave.h` | saves and the config record on the VMU (DC4): the card on screen, what is known about the VMU, the config write-behind |
| `src/vms.c`, `src/vms.h` | VMS files and the VMU file system, portable C: atomic replace, partial reads |
| `src/save_icon.c` | the save's icon and eyecatch: the Boreagotchi from the save |
| `src/misc.c` | the harness mailbox `dc_mailbox`, logging (dbgio + mailbox ring), `plat_ticks`, `plat_entropy`, `plat_name` ("DC") |
| `src/hcmd.c` | harness commands (save, load, scene, seed, text probe, ...), the GBA's ids and meaning |
| `src/dc2d.h`, `src/gfx2d.c`, `src/texcache.c`, `src/font.c`, `src/pal.c` | the 2D layer (DC3), portable C: prim lists, textures, glyph pages, palettes |
| `src/pvr2d.c` | the 2D layer's PVR backend: init, textures, palettes, submit |
| `src/audio.c` | the sound (DC5): `plat_tone`, `plat_sample*`, the KOS stream and its thread, the mailbox counters |
| `src/mixer.c`, `src/sfx.c`, `src/audio_int.h` | the software mixer and the effects bank (DC5), portable C: a slot of PCM per logic tick, copied out by the stream |
| `src/dc3d.h`, `src/dc3d.c` | the 3D batches (DC7), portable C: projected polygons in the 2D list's painter's order, segments, markers, near clipping, the held batch |
| `src/overworld.c` | `plat_overworld` (DC7), portable C: terrain, blocks, sea, billboards, shadows, light, tints |
| `src/pvr3d.c` | the 3D batches' PVR backend: headers, strips, the shadows' blob texture |
| `src/room.c`, `src/room_int.h` | `plat_room` and `plat_room_project` (DC8), portable C: the house as room_ref draws it, the shell as one cached texture, pieces and residents as sprites in room_ref's order (O(n^2)), ghosts, the selection glow, the zoom |
| `src/plat_stub.c` | the placeholder for the platform call DC6 implements (`plat_view3d`) |
| `src/dc_plat.h` | the internal interfaces, `DcMailbox` |
| `ip.txt` | `makeip` input: original title and maker strings |
| `tools/mkcdi.sh` | ELF -> 1ST_READ.BIN -> ISO with IP.BIN -> CDI |
| `tools/elfsize.sh`, `tools/check_elf.sh` | RAM budget report; ELF checks (mailbox symbol, entry point) |
| `tools/setup_kos.sh`, `tools/kos_pins.env` | the toolchain install at pinned versions (DC0b) |
| `tests/boot.txt` | harness script: boot, BOOT -> TITLE, 30 ticks per second |
| `tests/gfx2d/` | the 2D layer's host test: unit tests, the dual-render runner (`tee.c`), the software PVR (`swpvr.c`), `run_compare.sh` |
| `tests/vmu/` | DC4: `test_vmu.c` (host: VMS, the file system on a simulated VMU pulled out at every write, icons, controls), the Flycast scripts and `run_flycast.sh` |
| `tests/overworld/` | DC7: `run_flycast.sh` (the overworld scripts on Flycast, the native runner and the host's software PVR), `compare3d.py`, `sheet.py`, the Hellvile walks |
| `tests/house/` | DC8: `run_flycast.sh` (room_ref's golden frames drawn by the game on Flycast; the house scripts on Flycast, the host's software PVR and the native runner), `compare_house.py`; the host test is `tests/gfx2d/test_dc_room.c` |
| `tests/audio/` | DC5: `test_audio.c` (host: the mixer's units), `sndtee.c` + `run_compare.sh` (the mixer against musrender on runner scripts), `compare.py`, the Flycast scripts and `run_flycast.sh` |
