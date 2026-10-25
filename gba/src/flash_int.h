/* flash_int.h - FLASH chip primitives (flash_ram.c), offsets are within
 * the current 64 KB bank (owner: gba).
 *
 * The primitives are ARM code in the ROM block __flash_ram_start ..
 * __flash_ram_end (gba.ld) and must run from RAM: never call them
 * directly. save_flash.c copies the block onto the stack (FL_RAM_WORDS
 * words, a local of each save / load entry point) and calls the copy
 * through FL(fn). */
#ifndef GBA_FLASH_INT_H
#define GBA_FLASH_INT_H

#include "gba_plat.h"

uint16_t fl_read_id(void);
void fl_bank(int bank);
int  fl_erase_sector(uint32_t off, int macronix);
int  fl_program(uint32_t off, const uint8_t *src, uint32_t n, int macronix);
int  fl_program_atmel_page(uint32_t off, const uint8_t *src);
void fl_read(uint32_t off, uint8_t *dst, uint32_t n);

/* the block in ROM (gba.ld ASSERTs it fits FL_RAM_WORDS) */
extern const uint32_t __flash_ram_start[], __flash_ram_end[];
#define FL_RAM_WORDS 288                  /* 1,152 B of stack while saving */

#endif
