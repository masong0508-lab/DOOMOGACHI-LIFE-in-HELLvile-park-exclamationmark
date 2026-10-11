/* flash_int.h - FLASH chip primitives (flash.iwram.c), offsets are within
 * the current 64 KB bank (owner: gba). */
#ifndef GBA_FLASH_INT_H
#define GBA_FLASH_INT_H

#include "gba_plat.h"

uint16_t fl_read_id(void) IWRAM_CALL;
void fl_bank(int bank) IWRAM_CALL;
int  fl_erase_sector(uint32_t off, int macronix) IWRAM_CALL;
int  fl_program(uint32_t off, const uint8_t *src, uint32_t n, int macronix) IWRAM_CALL;
int  fl_program_atmel_page(uint32_t off, const uint8_t *src) IWRAM_CALL;
void fl_read(uint32_t off, uint8_t *dst, uint32_t n) IWRAM_CALL;

#endif
