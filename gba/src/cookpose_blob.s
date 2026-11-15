@ cookpose_blob.s - Granny's baked poses in the ROM (S2-4c; owner: gba; cookpose.c reads it).
@
@ build/gen/poses.bin is written at build time by the native runner (runner --bake-poses,
@ core/native/cookpose_native.c): every pose the kept kitchen draws Granny in at 240x160.
@ The Makefile passes its path as GBA_POSES_BIN and makes this object depend on it.
    .section .rodata.poses, "a", %progbits
    .align 2
    .global gba_poses
gba_poses:
    .incbin GBA_POSES_BIN
    .align 2
