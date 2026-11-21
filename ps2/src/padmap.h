/* PS2 pad -> shared C99 input mask. No PS2SDK dependency: host-testable. */
#ifndef DOOMO_PS2_PADMAP_H
#define DOOMO_PS2_PADMAP_H
#include <stdint.h>

/* PS2SDK padButtonStatus.btns is active LOW; callers invert it first.
 * These are the documented DualShock button bits, NOT the core mask. */
enum {
    PS2_KEY_SELECT=0x0001, PS2_KEY_L3=0x0002, PS2_KEY_R3=0x0004,
    PS2_KEY_START=0x0008, PS2_KEY_UP=0x0010, PS2_KEY_RIGHT=0x0020,
    PS2_KEY_DOWN=0x0040, PS2_KEY_LEFT=0x0080, PS2_KEY_L2=0x0100,
    PS2_KEY_R2=0x0200, PS2_KEY_L1=0x0400, PS2_KEY_R1=0x0800,
    PS2_KEY_TRIANGLE=0x1000, PS2_KEY_CIRCLE=0x2000,
    PS2_KEY_CROSS=0x4000, PS2_KEY_SQUARE=0x8000
};
uint32_t ps2_buttons_to_core(uint16_t pressed);
#endif
