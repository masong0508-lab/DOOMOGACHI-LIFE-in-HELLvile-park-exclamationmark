@ talk_blob.s - the Talk word network + grammar in the ROM (owner: gba; plat_talk_gba.c reads it).
@
@ build/gen/TALK_en.BIN is written at build time by core/tools/talkc.py (the same file the native
@ runner reads). The ROM is memory-mapped, so Talk reads it in place: no RAM is spent on it.
@ The Makefile passes its path as GBA_TALK_BIN and makes this object depend on it.
    .section .rodata.talk, "a", %progbits
    .align 2
    .global gba_talk_bin
    .global gba_talk_bin_end
gba_talk_bin:
    .incbin GBA_TALK_BIN
gba_talk_bin_end:
    .align 2
