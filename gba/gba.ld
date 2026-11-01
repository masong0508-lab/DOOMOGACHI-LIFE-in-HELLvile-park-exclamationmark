/* gba.ld - linker script for the DOOMOGACHI LIFE GBA ROM (owner: gba).
 *
 *   ROM    0x08000000  32 MB  header (.crt0), code (THUMB), const data
 *   EWRAM  0x02000000 256 KB  mailbox (80 bytes, test harness), .data +
 *                             .bss (core state: doomo_town,
 *                             doomo_map, ...) + platform caches
 *   IWRAM  0x03000000  32 KB  .iwram (ARM hot code + fast data, copied
 *                             from ROM at boot), the overlay slot (one
 *                             code group at a time, iwram_ovl.c; G4),
 *                             .iwram_bss, stacks
 *
 * IWRAM layout (top):
 *   0x03007FF0..0x03007FFF  BIOS (IRQ vector, IF mirror, ...)
 *   0x03007FA0..0x03007FEF  SVC stack (BIOS calls)
 *   0x03007D00..0x03007F9F  IRQ stack (672 bytes)
 *   __sp_usr = 0x03007D00   system/user stack, grows down
 *   __iwram_stack_limit     iwram code/data must end below this so the
 *                           main stack keeps GBA_STACK_SIZE bytes
 *
 * Whole translation units named *.iwram.c (compiled -marm) land in IWRAM
 * (.text/.rodata/.data -> .iwram, .bss -> .iwram_bss). Single functions
 * elsewhere use IWRAM_CODE (gba_hw.h) = section(".iwram").
 * Exceptions (the overlay slot, below): r3d.iwram.c's code + const data
 * and section .iwram_3d (r3d_loops.s) form the 3D group, section
 * .iwram_2d (IWRAM_2D_CODE, iwram_ovl.h) the 2D group.
 */
OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

GBA_STACK_SIZE = 0x1800;   /* 6 KB main stack */

MEMORY {
    rom   (rx)  : ORIGIN = 0x08000000, LENGTH = 32M
    ewram (rwx) : ORIGIN = 0x02000000, LENGTH = 256K
    iwram (rwx) : ORIGIN = 0x03000000, LENGTH = 32K
}

__sp_usr = 0x03007D00;
__iwram_stack_limit = __sp_usr - GBA_STACK_SIZE;

SECTIONS
{
    .crt0 : {
        KEEP(*(.crt0))
        . = ALIGN(4);
    } > rom

    .text : {
        /* the flash chip routines (flash_ram.c, ARM): one position
         * independent block that save_flash.c copies onto the stack for
         * each save / load call (G0, task #78: not IWRAM for good) */
        . = ALIGN(4);
        __flash_ram_start = .;
        KEEP(*(.text.flashram))
        . = ALIGN(4);
        __flash_ram_end = .;
        /* ROM_COLD (gba_hw.h): cold functions of *.iwram.c files stay
         * in ROM; listed first so the .iwram rule below never sees them */
        *(.text.rom .text.rom.*)
        *(EXCLUDE_FILE(*.iwram.o) .text.unlikely EXCLUDE_FILE(*.iwram.o) .text.unlikely.*)
        *(EXCLUDE_FILE(*.iwram.o) .text EXCLUDE_FILE(*.iwram.o) .text.* .gnu.linkonce.t.*)
        *(.glue_7) *(.glue_7t) *(.vfp11_veneer) *(.v4_bx)
        . = ALIGN(4);
    } > rom

    .rodata : {
        *(EXCLUDE_FILE(*.iwram.o) .rodata EXCLUDE_FILE(*.iwram.o) .rodata.* .gnu.linkonce.r.*)
        . = ALIGN(4);
    } > rom

    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > rom
    .ARM.exidx : {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > rom

    .preinit_array : { KEEP(*(.preinit_array)) } > rom
    .init_array : { KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array)) } > rom
    .fini_array : { KEEP(*(.fini_array)) } > rom
    . = ALIGN(4);

    /* ---- IWRAM: ARM code + fast data, loaded from ROM ---------------- */
    .iwram : {
        __iwram_start = .;
        *(.iwram .iwram.*)
        EXCLUDE_FILE(*r3d.iwram.o) *.iwram.o(.text .text.* .rodata .rodata.* .data .data.*)
        *.iwram.o(.data .data.*)
        *(.iwram_data .iwram_data.*)
        . = ALIGN(4);
        __iwram_end = .;
    } > iwram AT > rom
    __iwram_lma = LOADADDR(.iwram);

    /* ---- IWRAM code overlay (G4, iwram_ovl.c) ------------------------
     * One slot, right after the resident code, holds ONE group at a time;
     * iwram_ovl_use() DMAs a group in from ROM on a mode change (crt0
     * copies none of them). The slot is as big as the largest group.
     *   .ovl_3d  the raycaster's hot code: r3d.iwram.c's code and const
     *            data + r3d_loops.s (section .iwram_3d); only plat_view3d()
     *            runs it
     *   .ovl_2d  the tile-mode 2D loops (section .iwram_2d; G5's canvas)
     *   .ovl_room (H8) the house's lamp-pool loops (section .iwram_room;
     *            room.c, plat_room() loads it)
     * NOCROSSREFS: code in one group can never reference the other.
     * Data that must survive a swap (.data, .bss) never goes in here. */
    . = ALIGN(4);
    __iwram_ovl_start = .;
    OVERLAY : NOCROSSREFS AT (__iwram_lma + SIZEOF(.iwram)) {
        .ovl_3d {
            *r3d.iwram.o(.text .text.* .rodata .rodata.*)
            *(.iwram_3d .iwram_3d.*)
            . = ALIGN(4);
        }
        .ovl_2d {
            *(.iwram_2d .iwram_2d.*)
            . = ALIGN(4);
        }
        .ovl_room {
            *(.iwram_room .iwram_room.*)
            /* never empty: an empty last section of the OVERLAY makes ld
             * put the location counter (.iwram_bss) at the end of the one
             * before it, inside the slot (the engine-test ROM links no
             * room.c) */
            LONG(0)
            . = ALIGN(4);
        }
    } > iwram
    __iwram_ovl_end = .;
    /* OVERLAY stores the groups one after the other in ROM (an empty
     * group has no LOADADDR of its own, so the addresses are summed) */
    __iwram_ovl_3d_lma = __iwram_lma + SIZEOF(.iwram);
    __iwram_ovl_3d_size = SIZEOF(.ovl_3d);
    __iwram_ovl_2d_lma = __iwram_ovl_3d_lma + SIZEOF(.ovl_3d);
    __iwram_ovl_2d_size = SIZEOF(.ovl_2d);
    __iwram_ovl_room_lma = __iwram_ovl_2d_lma + SIZEOF(.ovl_2d);
    __iwram_ovl_room_size = SIZEOF(.ovl_room);
    __iwram_ovl_lma_end = __iwram_ovl_room_lma + SIZEOF(.ovl_room);

    .iwram_bss (NOLOAD) : {
        __iwram_bss_start = .;
        *(.iwram_bss .iwram_bss.*)
        *.iwram.o(.bss .bss.* COMMON)
        . = ALIGN(4);
        __iwram_bss_end = .;
    } > iwram

    /* ---- EWRAM: everything else ------------------------------------- */
    /* Test-harness mailbox, fixed at 0x02000000 (gba/harness reads the
     * logic tick counter from here and posts commands). 80 bytes. */
    .mailbox (NOLOAD) : {
        KEEP(*(.mailbox))
        . = ALIGN(16);
    } > ewram

    /* after the overlay groups' ROM copies (OVERLAY ... AT() does not
     * advance the rom region, so AT > rom would land on top of them) */
    .data : AT (__iwram_ovl_lma_end) {
        __data_start = .;
        *(EXCLUDE_FILE(*.iwram.o) .data EXCLUDE_FILE(*.iwram.o) .data.* .gnu.linkonce.d.*)
        *(.ewram_data .ewram_data.*)
        . = ALIGN(4);
        __data_end = .;
    } > ewram
    __data_lma = LOADADDR(.data);

    .bss (NOLOAD) : {
        __bss_start = .;
        *(EXCLUDE_FILE(*.iwram.o) .bss EXCLUDE_FILE(*.iwram.o) .bss.* .gnu.linkonce.b.*)
        *(.ewram_bss .ewram_bss.*)
        *(EXCLUDE_FILE(*.iwram.o) COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > ewram

    .noinit (NOLOAD) : {
        *(.noinit .noinit.*)
        . = ALIGN(4);
        __ewram_end = .;
    } > ewram

    __rom_end = __data_lma + SIZEOF(.data);

    /DISCARD/ : { *(.ARM.attributes) *(.comment) *(.note.*) }

    .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
    .debug_abbrev   0 : { *(.debug_abbrev) }
    .debug_line     0 : { *(.debug_line .debug_line.* .debug_line_end) }
    .debug_frame    0 : { *(.debug_frame) }
    .debug_str      0 : { *(.debug_str) }
    .debug_loc      0 : { *(.debug_loc) }
    .debug_aranges  0 : { *(.debug_aranges) }
    .debug_ranges   0 : { *(.debug_ranges) }
    .debug_rnglists 0 : { *(.debug_rnglists) }
    .debug_loclists 0 : { *(.debug_loclists) }
    .debug_line_str 0 : { *(.debug_line_str) }
}

ASSERT(__iwram_bss_end <= __iwram_stack_limit,
       "IWRAM overflow: code/data eat into the 6 KB main stack")
/* the slot holds the largest group (the stack check above sees it) */
ASSERT(__iwram_ovl_end - __iwram_ovl_start >= __iwram_ovl_3d_size &&
       __iwram_ovl_end - __iwram_ovl_start >= __iwram_ovl_2d_size &&
       __iwram_ovl_end - __iwram_ovl_start >= __iwram_ovl_room_size,
       "IWRAM overlay slot smaller than a group")
/* G6: the overworld billboards' tables use the slot's bytes past the 2D
 * group while it is in (ow_obj.c BB_TAIL_BYTES, iwram_ovl_2d_tail): a 2D
 * group that grows into them must shrink ow_obj.c's tables first */
ASSERT(__iwram_ovl_end - __iwram_ovl_start - __iwram_ovl_2d_size >= 1712,
       "IWRAM overlay: the 2D group leaves under 1,712 B for ow_obj.c (BB_TAIL_BYTES)")
ASSERT(__iwram_ovl_3d_size == 0 || LOADADDR(.ovl_3d) == __iwram_ovl_3d_lma,
       "IWRAM overlay: 3D group ROM copy moved")
ASSERT(__iwram_ovl_2d_size == 0 || LOADADDR(.ovl_2d) == __iwram_ovl_2d_lma,
       "IWRAM overlay: 2D group ROM copy moved")
ASSERT(__iwram_ovl_room_size == 0 || LOADADDR(.ovl_room) == __iwram_ovl_room_lma,
       "IWRAM overlay: room group ROM copy moved")
/* FL_RAM_WORDS (flash_int.h): the stack copy of the flash routines */
ASSERT(__flash_ram_end - __flash_ram_start <= 1152,
       "flash_ram.c grew past its 1,152 B stack copy (FL_RAM_WORDS)")
ASSERT(__ewram_end <= 0x02040000, "EWRAM overflow (256 KB)")
