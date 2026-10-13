/* input.h - buttons. The platform passes a bitmask to doomo_frame() every
 * tick; the core tracks edges and auto-repeat.
 *
 * GBA:  D-pad, A, B, L, R, START, SELECT. BTN_X = SELECT+A, BTN_Y = SELECT+B
 *       are synthesised by the core only when the platform sets
 *       DOOMO_INPUT_SYNTH_XY (GBA does). L2/R2 are not available.
 * PS1:  Cross=A, Circle=B, Square=X, Triangle=Y, L1=L, R1=R, L2, R2,
 *       START, SELECT.
 */
#ifndef DOOMO_INPUT_H
#define DOOMO_INPUT_H

#include "types.h"

enum {
    BTN_UP = 1 << 0, BTN_DOWN = 1 << 1, BTN_LEFT = 1 << 2, BTN_RIGHT = 1 << 3,
    BTN_A = 1 << 4, BTN_B = 1 << 5, BTN_L = 1 << 6, BTN_R = 1 << 7,
    BTN_START = 1 << 8, BTN_SELECT = 1 << 9,
    BTN_X = 1 << 10, BTN_Y = 1 << 11, BTN_L2 = 1 << 12, BTN_R2 = 1 << 13
};
#define DOOMO_INPUT_SYNTH_XY (1u << 31)   /* flag OR-ed into the mask */

void input_update(uint32_t raw);     /* called by doomo_frame()          */
int  btn_down(uint32_t b);           /* any of b held                    */
int  btn_pressed(uint32_t b);        /* any of b went down this tick     */
int  btn_released(uint32_t b);
int  btn_repeat(uint32_t b);         /* pressed, then every 3 ticks after 12 */
uint32_t btn_held_ticks(uint32_t b); /* how long the (first) button is held */
/* Swallow all input this tick (e.g. after a scene change). Buttons still
 * held afterwards are ignored until they are released. */
void input_eat(void);

/* ---- analog sticks (PS1 DualShock) ----------------------------------------
 * Platforms with analog sticks call input_set_analog() before doomo_frame().
 * Axes are -128..127 (0 = centred, +x right, +y DOWN like the pad reports).
 * stick_mask = the BTN_ bits the platform synthesised from the left stick
 * (so the shooter can tell stick from D-pad). Platforms that never call it
 * (GBA, native) keep the exact old digital behaviour. */
void input_set_analog(int present, int lx, int ly, int rx, int ry, uint32_t stick_mask);
int  input_analog(int *lx, int *ly, int *rx, int *ry);   /* 1 = analog pad present */
uint32_t input_stick_mask(void);
/* "Smooth feel": eased turning, fire buffering, aim friction. Off by default;
 * the PS1 port turns it on. */
void input_set_feel(int on);
int  input_feel(void);

/* ---- foundation additions ---- */
void input_reset(void);              /* forget all state (tests, boot)   */

#endif
