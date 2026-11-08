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
| `compare.py` | DC3: Dreamcast shots (640x480, or `--half`) against the native runner's 320x240 shots of the same script, in RGB565, with diff pictures and a gate |
| `ui_compare.sh` | DC3: the six UI scripts in `dcrun` and in the native runner, then `compare.py` (`make -C dc ui-compare`) |
| `vmu_check.py` | DC4: checks a VMU flash image (Flycast's `system/dc/vmu_save_A1.bin`): FAT, directory, VMS headers and CRCs; `--fill N` adds another game's file, `--icon` draws a file's icon frames and eyecatch |
| `known_diffs.tsv` | DC3: the expected differences of those scripts, each with its reason (a platform label, a Dreamcast-only option row, the 3D view that is DC6's) |

## Build and test

```sh
make -C dc/harness            # build/dcrun
make -C dc/harness test       # dcrun's self-test against tests/mockcore.c
make -C dc/harness flycast    # build/flycast_libretro.so (estimated 10-20 min at -j2; not yet timed)
```

`dc/Makefile` also finds the core at `<toolchain prefix>/flycast_libretro.so`
(next to `kos/`, e.g. `/opt/toolchains/dc/flycast_libretro.so`, pinned
commit in `flycast_libretro.commit`), or wherever `FLYCAST=` points.

Host packages (Ubuntu 24.04): `build-essential libegl-dev libgl-dev
libegl-mesa0 libgl1-mesa-dri` (Mesa's EGL with `EGL_MESA_platform_surfaceless`
and the llvmpipe GL driver: no GPU and no X server needed). Building Flycast
also needs `git` and `g++`.

`build_flycast.sh` pins Flycast to `e7085e20fe8d81257e11aea5f19a9c1ca2b1bd55`
(2026-10-31), fetches it with a shallow `git fetch`, builds with
`make platform=unix` and copies the upstream output (`parallel_dc_libretro.so`,
the pinned Makefile's `TARGET_NAME`) to `build/flycast_libretro.so`. It deletes the source and object
trees afterwards (also when the build fails) unless `--keep` is given, and it is a no-op when the `.so`
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
| `--sync` | `sync on 0` before the script: the game holds before its first tick, where the native runner starts, and every wait is tick-exact (see `sync`) |
| `--native-seed N` | `--sync`, and at that first hold the game gets the start `runner --seed N` gets (0x5EED1234 is the runner's default): `plat_entropy()` pinned to N, `doomo_rng` and the start town redone as `doomo_init()` makes them (mailbox command 17). The console boots on real entropy before the harness can reach it, so this is how a Dreamcast run starts in the native runner's state |
| `--console` | DC4: leave the mailbox's `test_flags` at 0, as a real console (by default dcrun sets `PLAT_TEST_NO_PICKER`, so the first boot of an empty VMU skips the language picker, and `PLAT_TEST_NO_TIPS` (CL4), so the first-time tip cards stay off, as the native runner does) |
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
| `press BTN..` | down 1 tick, up 1 tick in sync mode (the native runner's press); without sync down 2 ticks, so the maple poll cannot miss it, then up 1 |
| `hold BTN.. N` / `down BTN..` / `release [BTN..]` | as the native runner |
| `shot NAME` | write `SHOTS/NAME.png` |
| `log TEXT` / `expect-log TEXT` | print / fail unless TEXT is in the game's log so far |
| `tips on` / `tips off` | CL4: clear / set `PLAT_TEST_NO_TIPS` in the mailbox's `test_flags`, so the first-time tip cards show in this run (the native runner's and the GBA harness's `tips on`) / stay off (the default) |
| `expect-scene NAME` | the mailbox's last scene, or a `scene: set NAME` log line |
| `mark-ticks` / `expect-ticks LO HI` | fail unless the game's tick count advanced by LO..HI since `mark-ticks` (the 30 Hz check: `frames 300` must give about 150) |
| `save N` / `load N` / `expect-save N` / `verify N` | mailbox commands, run in the game; a result of 0 fails (the GBA convention: nonzero = success) |
| `save-must-fail N` | DC4: the save must be refused (result 0) |
| `town-fill SEED` / `town-crc` / `expect-town-crc` | DC4: the GBA's save-test commands (mailbox 4 and 5): a worst-case island (SEED, or SEED with bit 31 for a lived-in story island), remember the CRC of everything a save stores, fail unless it is the same |
| `shot-during FRAMES NAME` | DC4: the next mailbox command takes shot NAME FRAMES video frames in, while the game is still busy with it (the VMU card of a save) |
| `scene NAME [ARG]` / `push NAME [ARG]` / `seed N` / `expect-flag F [V]` | mailbox commands |
| `audio-reset` / `audio [LABEL]` / `expect-audio MIN [LABEL]` | peak-to-peak of the 16-bit output since `audio-reset` |
| `expect-underruns MAX` / `expect-sfx MIN` / `audio-stats [LABEL]` | DC5: the game's sound underruns plus late AICA refills (mailbox `snd_underruns` + `snd_starved`) since `audio-reset` must be at most MAX / the sound effects it started (`snd_started`) at least MIN / print the game's sound counters (slots, ring, underruns, gaps, clips, effects, the content hash, the mixer's time per tick, the longest refill interval) |
| `port N` | later button, stick and trigger commands act on port N (1-4) |
| `device N pad\|keyboard\|mouse\|none` | plug a device into port N |
| `stick X Y` | left stick, -128..127 |
| `trigger L\|R V` | analog trigger, 0..255 |
| `type "TEXT"` | the native runner's `type`: each character goes to the open text entry (`ui_text_entry_feed`, mailbox command 16), then 1 tick; `\n` is OK, `\b` Backspace; fails when no text entry is open |
| `key "TEXT"` | type on the Dreamcast keyboard (a port set to `keyboard`), one key every 2 ticks (`\n` Return, `\b` Backspace) |
| `sync on [TICK]` / `sync off` | tick-exact waits, as `gba/harness`: the game holds at the top of its loop, before it reads the pad, whenever its tick count equals the mailbox's `hold_at`; every wait moves `hold_at` on and runs until the game holds again. When the input changed while held, the hold lasts 3 more video frames first (KOS's maple poll runs once a vblank), so the game reads it on the tick it was given on. The first hold is at absolute tick TICK (default: the next). Shots taken while held run 2 frames first, so the last tick's picture is on screen. In sync mode `draw_all` is set, so a held game never skips a draw to catch up |
| `mouse DX DY [L][R][M]` | move the mouse on the current port, buttons held for a tick |
| `expect-rumble [PORT]` | fail unless rumble switched on since the start or the last `expect-rumble` |
| `vmu-shot NAME` | VMU A1's 48x32 LCD to `SHOTS/NAME.png` (Flycast's VMU overlay, on for one frame) |

Mailbox commands (`scene`, `save`, `seed`, `type`, ...) wait for the game's
mailbox magic first, so a script may start with one.

Buttons: `A B X Y START UP DOWN LEFT RIGHT L R`; `SELECT` is START+Y (the
port's convention). Flycast maps the RetroPad's B, A, Y, X to the
Dreamcast's A, B, X, Y; `dcrun` uses Dreamcast names.

Each rumble change prints `rumble: tick T port P strong|weak S`.

After every video frame `dcrun` also reads the game's work counters: a
tick whose work took more than 33.3 ms (the 30 fps budget) prints `work:
tick T took N us`, and the run ends with the number of such ticks and the
worst one (`dcrun: work over 33333 us on K of N ticks; worst ...`), next
to the 2D layer's high-water marks (prims and vertex bytes in one frame).
The work is the game's own `timer_us_gettime64()` from the start of
`plat_begin_frame` to the end of `plat_end_frame`'s submit, in emulated time.

## The mailbox

`dc_mailbox.h` is the contract with the game: one `volatile DcMailbox
dc_mailbox` in main RAM, with the same fields as the GBA's `GbaMailbox`
(magic, ticks, drawn, command slot, profiling, `name[20]`, `hold_at`,
`draw_all`) plus the last scene name and a 4 KB `plat_log` ring. `dcrun`
reads it through `RETRO_MEMORY_SYSTEM_RAM` at `addr & 0xFFFFFF`, polls the
log ring after every video frame and posts commands by writing `arg`, then
`cmd`, and running until the game clears `cmd`. The command numbers are the
GBA's (1-13; 14 and 15 are the GBA's house commands); 16 (`type`) and 17
(`--native-seed`) are the Dreamcast's own.

## Limits

- Flycast is not cycle-accurate: speed in the harness is a rough guide only.
- Flycast cannot open an ELF over 16 MiB ("Failed to open"); the game's ELF
  with debug info is about 17 MB, so `make -C dc test` boots
  `build/doomogachi-nodebug.elf` (`strip --strip-debug`) and the CDI, and
  takes the symbols from `build/doomogachi.nm`.
- `vmu-shot` reads Flycast's VMU overlay, which draws the LCD with the
  overlay's colours (pixel on: black, off: Flycast's default).
- Mouse buttons are held for one tick per `mouse` command.
