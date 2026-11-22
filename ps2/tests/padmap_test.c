/* Run on host: cc -std=c99 -Wall -Wextra -Werror -Icore/include ... */
#include <assert.h>
#include <stdio.h>
#include "../src/padmap.h"
#include "doomo/input.h"

int main(void)
{
    static const struct { uint16_t raw; uint32_t core; } map[] = {
        {PS2_KEY_UP, BTN_UP}, {PS2_KEY_DOWN, BTN_DOWN},
        {PS2_KEY_LEFT, BTN_LEFT}, {PS2_KEY_RIGHT, BTN_RIGHT},
        {PS2_KEY_CROSS, BTN_A}, {PS2_KEY_CIRCLE, BTN_B},
        {PS2_KEY_SQUARE, BTN_X}, {PS2_KEY_TRIANGLE, BTN_Y},
        {PS2_KEY_L1, BTN_L}, {PS2_KEY_R1, BTN_R},
        {PS2_KEY_L2, BTN_L2}, {PS2_KEY_R2, BTN_R2},
        {PS2_KEY_L3, BTN_L3}, {PS2_KEY_R3, BTN_R3},
        {PS2_KEY_START, BTN_START}, {PS2_KEY_SELECT, BTN_SELECT}
    };
    unsigned i;
    assert(ps2_buttons_to_core(0) == 0);
    for (i = 0; i < sizeof(map)/sizeof(map[0]); i++) {
        assert(ps2_buttons_to_core(map[i].raw) == map[i].core);
        assert(ps2_buttons_to_core((uint16_t)~map[i].raw) ==
               (0xffffu ^ map[i].core));
    }
    assert(ps2_buttons_to_core(0xffffu) == 0xffffu);
    assert(ps2_buttons_to_core(PS2_KEY_CROSS | PS2_KEY_DOWN | PS2_KEY_R1) ==
           (BTN_A | BTN_DOWN | BTN_R));
    assert(!ps2_pad_has_sticks(0x41, 4));
    assert(!ps2_pad_has_sticks(0x41, 32));
    assert(!ps2_pad_has_sticks(0x73, 7));
    assert(!ps2_pad_has_sticks(0x63, 8));
    assert(ps2_pad_has_sticks(0x53, 8));
    assert(ps2_pad_has_sticks(0x73, 8));
    assert(ps2_pad_has_sticks(0x79, 20));
    puts("PS2 padmap: all 16 buttons, active-low inversion and chords PASS");
    return 0;
}
