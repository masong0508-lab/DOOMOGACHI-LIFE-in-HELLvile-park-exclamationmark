# gba/ - DOOMOGACHI LIFE for the Game Boy Advance

A real GBA ROM running the shared C99 core. Full documentation (build,
mGBA, controls, memory map, renderers, saves, audio, harness, measured
performance): **[docs/GBA.md](../docs/GBA.md)**.

```sh
make -C gba                 # build/doomogachi.gba        the game
make -C gba engine-test     # build/doomogachi_et.gba     3D showcase tour (ENGINE_TEST=1)
make -C gba test            # headless mGBA tests: engine tour, flash save round trip, game smoke
make -C gba clean
mgba-qt gba/build/doomogachi.gba     # save type: Flash 1Mb (auto-detected)
```

Needs `gcc-arm-none-eabi` + newlib and `python3`; the test harness also
needs a host `gcc` and `libmgba-dev` (0.10).

Layout:

| path | what |
|------|------|
| `Makefile`, `gba.ld` | build (THUMB in ROM, `*.iwram.c` / `*.s` ARM in IWRAM), linker script with RAM `ASSERT`s |
| `src/crt0.s` | ROM header, boot, IRQ master handler, `FLASH1M_V103` tag |
| `src/main.c` | `main()`, 30 Hz pacing, input, log, ticks, entropy, mailbox |
| `src/video*.c`, `src/video_loops.s` | Mode 4 double buffer + the 2D primitives |
| `src/r3d*.c`, `src/r3d_loops.s` | `plat_view3d()`: height raycaster (ARM inner loops) |
| `src/overworld.c`, `src/ow*.c`, `src/ow_loops.s` | `plat_overworld()` |
| `src/audio.c`, `src/irq.iwram.c` | PSG tones, DirectSound sample mixer (VBlank IRQ) |
| `src/save_flash.c`, `src/flash_ram.c` | 128 KB flash saves (the chip routines run from a copy on the stack, G0) |
| `src/div.s`, `src/mem.s` | IWRAM integer division, `memcpy` / `memset` |
| `src/bigbuf.c` | the shared 17 KB EWRAM buffer (renderer caches, the core's save window: `plat_save_scratch()`) |
| `enginetest/` | ENGINE_TEST=1 tour, showcase map, save / division self-tests |
| `harness/` | headless libmgba runner (`build/harness`), test scripts |
| `tools/` | `gbafix.py` (header), `mktables.py` (tables), `memreport.py` (RAM budget) |
