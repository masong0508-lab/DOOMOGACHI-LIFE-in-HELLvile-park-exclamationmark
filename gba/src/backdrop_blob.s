@ backdrop_blob.s - the baked backdrops in the ROM (SP7; owner: gba; stage.c reads it).
@
@ build/gen/backdrops.bin is written at build time by the native runner
@ (core/build-style objects in build/native: runner --bake-backdrops), which draws every key
@ of every backdrop family (core/include/doomo/backdrop.h) with the core's own code at 240x160.
@ The Makefile passes its path as GBA_BACKDROPS_BIN and makes this object depend on it.
    .section .rodata.backdrops, "a", %progbits
    .align 2
    .global gba_backdrops
gba_backdrops:
    .incbin GBA_BACKDROPS_BIN
    .align 2
