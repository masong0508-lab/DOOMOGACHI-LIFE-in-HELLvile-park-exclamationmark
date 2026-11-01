# dc/harness: the headless Dreamcast harness

`dcrun` is a small libretro frontend. It loads the Flycast libretro core,
boots a Dreamcast program (`.elf`, `.cdi`, `.gdi` or `.chd`) with no window
and no Sega BIOS (Flycast's built-in HLE BIOS), and plays a runner script:
the same script language as the native runner and the GBA harness, plus
Dreamcast commands for 4 ports, analog sticks and triggers, keyboard,
mouse, rumble, the VMU screen and audio.

| file | what |
|---|---|
| `build_flycast.sh` | fetches libretro/flycast at a pinned commit and builds `build/flycast_libretro.so` |
| `dcrun.c` | the frontend: EGL surfaceless GL context, FBO readback, script player, mailbox |
| `dc_mailbox.h` | the RAM mailbox the game and the harness share (the game side arrives in DC2) |
| `png.c`, `png.h` | PNG writer (a copy of `gba/harness/png.c`) |
| `libretro.h` | the libretro API header (MIT), copied from the pinned Flycast tree |
| `tests/` | `mockcore.c` (a stand-in core) and `selftest.sh`: tests `dcrun` with no emulator |

## Build and test

```sh
make -C dc/harness            # build/dcrun
make -C dc/harness test       # dcrun's self-test against tests/mockcore.c
make -C dc/harness flycast    # build/flycast_libretro.so (estimated 10-20 min at -j2; not yet timed)
```

Host packages (Ubuntu 24.04): `build-essential libegl-dev libgl-dev
libegl-mesa0 libgl1-mesa-dri` (Mesa's EGL with `EGL_MESA_platform_surfaceless`
and the llvmpipe GL driver: no GPU and no X server needed). Building Flycast
also needs `git` and `g++`.

`build_flycast.sh` pins Flycast to `e7085e20fe8d81257e11aea5f19a9c1ca2b1bd55`
(2026-10-31), fetches it with a shallow `git fetch`, builds with
`make platform=unix HAVE_OPENMP=0` (one texture-decoding thread, so frames
repeat) and copies the `.so` to `build/`. It deletes the source and object
trees afterwards unless `--keep` is given, and it is a no-op when the `.so`
of the pinned commit is already there.

## Running

```sh
dc/harness/build/dcrun dc/harness/build/flycast_libretro.so dc/build/doomogachi.elf \
    --script dc/harness/scripts/boot.txt --shots /tmp/shots
```

| option | meaning |
|---|---|
| `--script FILE` | the runner script |
| `--shots DIR` | where `shot` and `vmu-shot` write PNGs (default `.`) |
| `--half` | write shots at 320x240 (2x2 box filter), to compare with the native runner's 320x240 |
| `--run-dir DIR` | the core's system and save directories (default `dc/harness/build/run`) |
| `--fresh` | empty the run directory first (VMU images included) |
| `--frames N` | run N video frames before the script |
| `--timeout SEC` | fail after SEC seconds of wall time (default 600) |
| `--mailbox ADDR` / `--nm FILE` | where `dc_mailbox` is (hex address, or `sh-elf-nm` output); for an `.elf` the harness runs `sh-elf-nm` itself (`$KOS_CC_BASE/bin` first) |
| `--wav FILE` | write all audio to a 16-bit stereo WAV |
| `--rtc EPOCH` / `--rtc now` | the Dreamcast clock at boot (default 2026-01-01 00:00 UTC, so runs repeat) |
| `--opt KEY=VALUE` | override a core option (Flycast's keep the `reicast_` prefix) |
| `--device N TYPE` | `pad`, `keyboard`, `mouse` or `none` on port N (default: 4 pads) |
| `--quiet` / `--verbose` | less output / also every core log line |

Exit status: 0 ok, 1 failed expectation, hang or timeout, 2 usage or load
error.

### Pinned core options

`dcrun` answers the core's option queries itself, so runs do not depend on
any RetroArch configuration: Dreamcast system, HLE BIOS, VGA cable (640x480
progressive), NTSC, USA region, English, internal resolution 1x, threaded
rendering off, frame skipping off, texture upscaling off, accurate GD-ROM
timing, the rumble pack (Puru Puru) on, shared VMU files (no per-game
copies), VMU beeps off. Everything else keeps the core's default. The full
list is `pinned_opts` in `dcrun.c`.

The Dreamcast real-time clock is read from the host's `time()`. `dcrun`
defines its own `time()` and is linked with `-rdynamic`, so the core binds
to it and boots with a fixed clock (`--rtc`).

### Run directory

| path | what |
|---|---|
| `RUN/system/dc/` | Flycast's data directory: `vmu_save_A1.bin` .. `D1` (the VMU images, kept between runs) |
| `RUN/saves/` | the core's save directory |

## Script

Times are logic ticks (30 Hz `doomo_frame` calls) from the game's mailbox,
as on the GBA. With no mailbox (a KallistiOS example) a tick is 2 video
frames, and the log is what the core logs.

| command | meaning |
|---|---|
| `wait N` / `frames N` | run N ticks / N raw 60 Hz video frames |
| `press BTN..` | hold 2 ticks, release, 1 more tick |
| `hold BTN.. N` / `down BTN..` / `release [BTN..]` | as the native runner |
| `shot NAME` | write `SHOTS/NAME.png` |
| `log TEXT` / `expect-log TEXT` | print / fail unless TEXT is in the game's log so far |
| `expect-scene NAME` | the mailbox's last scene, or a `scene: set NAME` log line |
| `save N` / `load N` / `expect-save N` / `verify N` | mailbox commands, run in the game |
| `scene NAME [ARG]` / `push NAME [ARG]` / `seed N` / `expect-flag F [V]` | mailbox commands |
| `audio-reset` / `audio [LABEL]` / `expect-audio MIN [LABEL]` | peak-to-peak of the 16-bit output since `audio-reset` |
| `port N` | later button, stick and trigger commands act on port N (1-4) |
| `device N pad\|keyboard\|mouse\|none` | plug a device into port N |
| `stick X Y` | left stick, -128..127 |
| `trigger L\|R V` | analog trigger, 0..255 |
| `type "TEXT"` / `key "TEXT"` | type on the keyboard, one key every 2 ticks (`\n` Return, `\b` Backspace) |
| `mouse DX DY [L][R][M]` | move the mouse on the current port, buttons held for a tick |
| `expect-rumble [PORT]` | fail unless rumble switched on since the start or the last `expect-rumble` |
| `vmu-shot NAME` | VMU A1's 48x32 LCD to `SHOTS/NAME.png` (Flycast's VMU overlay, on for one frame) |

Buttons: `A B X Y START UP DOWN LEFT RIGHT L R`; `SELECT` is START+Y (the
port's convention). Flycast maps the RetroPad's B, A, Y, X to the
Dreamcast's A, B, X, Y; `dcrun` uses Dreamcast names.

Each rumble change prints `rumble: tick T port P strong|weak S`.

## The mailbox

`dc_mailbox.h` is the contract with the game: one `volatile DcMailbox
dc_mailbox` in main RAM, with the same fields as the GBA's `GbaMailbox`
(magic, ticks, drawn, command slot, profiling, `name[20]`, `hold_at`,
`draw_all`) plus the last scene name and a 4 KB `plat_log` ring. `dcrun`
reads it through `RETRO_MEMORY_SYSTEM_RAM` at `addr & 0xFFFFFF`, polls the
log ring after every video frame and posts commands by writing `arg`, then
`cmd`, and running until the game clears `cmd`. The command numbers are the
GBA's.

## Limits

- Flycast is not cycle-accurate: speed in the harness is a rough guide only.
- `vmu-shot` reads Flycast's VMU overlay, which draws the LCD with the
  overlay's colours (pixel on: black, off: Flycast's default).
- Mouse buttons are held for one tick per `mouse` command.
