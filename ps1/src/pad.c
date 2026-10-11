/* pad.c - controller input through the BIOS pad driver (updated every
 * vblank). Digital pads, Dual Analog and DualShock (the left stick doubles
 * as the D-pad). Mapping (input.h): Cross=A, Circle=B, Square=X,
 * Triangle=Y, L1=L, R1=R, L2, R2, START, SELECT.
 */
#include <psxapi.h>
#include <psxpad.h>
#include "ps1.h"

static uint8_t s_padbuf[2][34];

void pad_init(void)
{
    /* StartPAD() is called by memcard_init() after InitCARD() (the BIOS
     * wants InitPAD -> InitCARD -> StartPAD -> StartCARD) */
    InitPAD(s_padbuf[0], 34, s_padbuf[1], 34);
}

static int s_lx = 128, s_ly = 128, s_rx = 128, s_ry = 128, s_analog;

int pad_analog(int *lx, int *ly, int *rx, int *ry)
{
    *lx = s_lx - 128; *ly = s_ly - 128; *rx = s_rx - 128; *ry = s_ry - 128;
    return s_analog;
}

uint32_t pad_read(void)
{
    const PADTYPE *p = (const PADTYPE *)s_padbuf[0];
    uint32_t out = 0;
    uint16_t b;
    s_analog = 0;
    if (p->stat == 0 && (p->type == PAD_ID_DIGITAL || p->type == PAD_ID_ANALOG ||
                         p->type == PAD_ID_ANALOG_STICK)) {
        b = (uint16_t)~p->btn;
        if (b & PAD_UP)       out |= BTN_UP;
        if (b & PAD_DOWN)     out |= BTN_DOWN;
        if (b & PAD_LEFT)     out |= BTN_LEFT;
        if (b & PAD_RIGHT)    out |= BTN_RIGHT;
        if (b & PAD_CROSS)    out |= BTN_A;
        if (b & PAD_CIRCLE)   out |= BTN_B;
        if (b & PAD_SQUARE)   out |= BTN_X;
        if (b & PAD_TRIANGLE) out |= BTN_Y;
        if (b & PAD_L1)       out |= BTN_L;
        if (b & PAD_R1)       out |= BTN_R;
        if (b & PAD_L2)       out |= BTN_L2;
        if (b & PAD_R2)       out |= BTN_R2;
        if (b & PAD_START)    out |= BTN_START;
        if (b & PAD_SELECT)   out |= BTN_SELECT;
        if (p->type == PAD_ID_ANALOG || p->type == PAD_ID_ANALOG_STICK) {
            s_analog = 1;
            s_lx = p->ls_x; s_ly = p->ls_y; s_rx = p->rs_x; s_ry = p->rs_y;
            if (s_lx < 128 - 48) out |= BTN_LEFT;
            if (s_lx > 128 + 48) out |= BTN_RIGHT;
            if (s_ly < 128 - 48) out |= BTN_UP;
            if (s_ly > 128 + 48) out |= BTN_DOWN;
        }
    }
    return out;
}
