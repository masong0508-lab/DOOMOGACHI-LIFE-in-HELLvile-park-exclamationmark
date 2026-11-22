#include "padmap.h"
#include "doomo/input.h"

/* Exactly the PS1 button meanings, with all 16 DualShock 2 buttons.
 * Input is active HIGH (the caller inverted the PADMAN status). */
uint32_t ps2_buttons_to_core(uint16_t p)
{
    uint32_t b = 0;
    if (p & PS2_KEY_UP)       b |= BTN_UP;
    if (p & PS2_KEY_DOWN)     b |= BTN_DOWN;
    if (p & PS2_KEY_LEFT)     b |= BTN_LEFT;
    if (p & PS2_KEY_RIGHT)    b |= BTN_RIGHT;
    if (p & PS2_KEY_CROSS)    b |= BTN_A;
    if (p & PS2_KEY_CIRCLE)   b |= BTN_B;
    if (p & PS2_KEY_SQUARE)   b |= BTN_X;
    if (p & PS2_KEY_TRIANGLE) b |= BTN_Y;
    if (p & PS2_KEY_L1)       b |= BTN_L;
    if (p & PS2_KEY_R1)       b |= BTN_R;
    if (p & PS2_KEY_L2)       b |= BTN_L2;
    if (p & PS2_KEY_R2)       b |= BTN_R2;
    if (p & PS2_KEY_L3)       b |= BTN_L3;
    if (p & PS2_KEY_R3)       b |= BTN_R3;
    if (p & PS2_KEY_START)    b |= BTN_START;
    if (p & PS2_KEY_SELECT)   b |= BTN_SELECT;
    return b;
}

int ps2_pad_has_sticks(uint8_t mode, unsigned packet_bytes)
{
    /* A basic digital reply (0x41, four bytes) ends before the sticks.
     * Accept the analog pad and DualShock replies, including pressure mode. */
    return packet_bytes >= 8 && (mode == 0x53 || mode == 0x73 || mode == 0x79);
}
