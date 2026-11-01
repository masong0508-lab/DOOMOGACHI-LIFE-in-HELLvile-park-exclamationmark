/* png.h - tiny PNG writer (owner: dreamcast). */
#ifndef DC_HARNESS_PNG_H
#define DC_HARNESS_PNG_H
#include <stdint.h>
/* rgb: w*h*3 bytes. Returns 1 on success. */
int png_write_rgb(const char *path, const uint8_t *rgb, int w, int h);
#endif
