# dc/ - BOREagotchi LIFE! in HELLvile PARK... for the Dreamcast

A self-booting Dreamcast disc running the shared C99 core on KallistiOS.
The port is being built in stages (the DC packages of the project plan);
full documentation: **[docs/DREAMCAST.md](../docs/DREAMCAST.md)**.

Today (DC2, the skeleton) the whole core is linked, boots, runs its 30 Hz
loop and changes scenes, with the harness mailbox and logging in place.
Nothing is drawn yet except the clear colour, there is no sound, and saves
go to a RAM disk: the 2D layer (DC3), input and VMU saves (DC4), audio
(DC5) and 3D (DC6) replace the placeholders in `src/plat_stub.c`.

## Build

```sh
dc/tools/setup_kos.sh --install-deps   # once: KallistiOS + sh-elf gcc 15 (about 40 min)
make dc                                # from the repo root: ELF + CDI
make -C dc                             # or: just the ELF (+ map, size report)
make -C dc disc                        # build/doomogachi.cdi (+ 1ST_READ.BIN, IP.BIN)
make -C dc test                        # ELF + CDI + check-elf (+ Flycast scripts, DC1)
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
| `src/main.c` | `KOS_INIT_FLAGS`, video (640x480, VGA or interlaced by cable), PVR init, the frame loop and its 30 Hz pacing, the controller |
| `src/misc.c` | the harness mailbox `dc_mailbox`, logging (dbgio + mailbox ring), `plat_ticks`, `plat_entropy`, `plat_name` ("DC") |
| `src/hcmd.c` | harness commands (save, load, scene, seed, text probe, ...), the GBA's ids and meaning |
| `src/plat_stub.c` | placeholders for the platform calls DC3-DC6 implement; saves on a RAM disk |
| `src/dc_plat.h` | the internal interfaces, `DcMailbox` |
| `ip.txt` | `makeip` input: original title and maker strings |
| `tools/mkcdi.sh` | ELF -> 1ST_READ.BIN -> ISO with IP.BIN -> CDI |
| `tools/elfsize.sh`, `tools/check_elf.sh` | RAM budget report; ELF checks (mailbox symbol, entry point) |
| `tools/setup_kos.sh`, `tools/kos_pins.env` | the toolchain install at pinned versions (DC0b) |
| `tests/boot.txt` | harness script: boot, BOOT -> TITLE, 30 ticks per second |
