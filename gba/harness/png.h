/* png.h - tiny PNG writer (owner: gba). */
#ifndef GBA_HARNESS_PNG_H
#define GBA_HARNESS_PNG_H
#include <stdint.h>
/* rgb: w*h*3 bytes. Returns 1 on success. */
int png_write_rgb(const char *path, const uint8_t *rgb, int w, int h);
#endif
