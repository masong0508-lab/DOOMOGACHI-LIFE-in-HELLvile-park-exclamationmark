/* gba.ld - linker script for the DOOMOGACHI LIFE GBA ROM (owner: gba).
 *
 *   ROM    0x08000000  32 MB  header (.crt0), code (THUMB), const data
 *   EWRAM  0x02000000 256 KB  .data + .bss (core state: doomo_town,
 *                             doomo_map, ...) + platform caches
 *   IWRAM  0x03000000  32 KB  .iwram (ARM hot code + fast data, copied
 *                             from ROM at boot), .iwram_bss, stacks
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
        *.iwram.o(.text .text.* .rodata .rodata.* .data .data.*)
        *(.iwram_data .iwram_data.*)
        . = ALIGN(4);
        __iwram_end = .;
    } > iwram AT > rom
    __iwram_lma = LOADADDR(.iwram);

    .iwram_bss (NOLOAD) : {
        __iwram_bss_start = .;
        *(.iwram_bss .iwram_bss.*)
        *.iwram.o(.bss .bss.* COMMON)
        . = ALIGN(4);
        __iwram_bss_end = .;
    } > iwram

    /* ---- EWRAM: everything else ------------------------------------- */
    .data : {
        __data_start = .;
        *(EXCLUDE_FILE(*.iwram.o) .data EXCLUDE_FILE(*.iwram.o) .data.* .gnu.linkonce.d.*)
        *(.ewram_data .ewram_data.*)
        . = ALIGN(4);
        __data_end = .;
    } > ewram AT > rom
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
ASSERT(__ewram_end <= 0x02040000, "EWRAM overflow (256 KB)")
