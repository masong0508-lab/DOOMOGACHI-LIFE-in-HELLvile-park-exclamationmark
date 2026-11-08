/* swpvr.h - the software PVR of the 2D layer's host tests (DC3; owner:
 * dreamcast). See swpvr.c. */
#ifndef DC_SWPVR_H
#define DC_SWPVR_H
#include <stdint.h>
#include "dc2d.h"
/* Render a submitted frame into rgb (640x480x3, RGB565 precision). */
void swpvr_render(const DcFrame *f, uint8_t *rgb);
uint32_t swpvr_tex_used(void);           /* video RAM in use by textures */
extern uint32_t swpvr_submits, swpvr_tex_peak;
#endif
