/* input.h - buttons. The platform passes a bitmask to doomo_frame() every
 * tick; the core tracks edges and auto-repeat.
 *
 * GBA:  D-pad, A, B, L, R, START, SELECT. BTN_X = SELECT+A, BTN_Y = SELECT+B
 *       are synthesised by the core only when the platform sets
 *       DOOMO_INPUT_SYNTH_XY (GBA does). L2/R2 are not available.
 * PS1:  Cross=A, Circle=B, Square=X, Triangle=Y, L1=L, R1=R, L2, R2,
 *       START, SELECT, L3 / R3 (the DualShock's stick clicks).
 */
#ifndef DOOMO_INPUT_H
#define DOOMO_INPUT_H

#include "types.h"

enum {
    BTN_UP = 1 << 0, BTN_DOWN = 1 << 1, BTN_LEFT = 1 << 2, BTN_RIGHT = 1 << 3,
    BTN_A = 1 << 4, BTN_B = 1 << 5, BTN_L = 1 << 6, BTN_R = 1 << 7,
    BTN_START = 1 << 8, BTN_SELECT = 1 << 9,
    BTN_X = 1 << 10, BTN_Y = 1 << 11, BTN_L2 = 1 << 12, BTN_R2 = 1 << 13,
    BTN_L3 = 1 << 14, BTN_R3 = 1 << 15   /* (ST1) stick clicks: PS1 DualShock only */
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
/* ---- (ST1) real stick support ----------------------------------------------
 * Caps of the analog pad in the player's hands (only meaningful while
 * input_analog() says one is present). input_set_analog() resets them to
 * INPUT_CAP_RSTICK (the DualShock); the Dreamcast calls input_set_caps()
 * after it each tick with its analog triggers (0..255). */
enum { INPUT_CAP_RSTICK = 1, INPUT_CAP_TRIGGERS = 2, INPUT_CAP_PRESENT = 4 };
/* Compile-time: DOOMO_STICKS 0 on the GBA (no analog pad: the stick code
 * folds away), DOOMO_TRIGGERS 1 only where a pad has analog triggers (the
 * Dreamcast; the native runner and tests). */
#if defined(DOOMO_PLATFORM_GBA)
#define DOOMO_STICKS 0
#else
#define DOOMO_STICKS 1
#endif
#if defined(DOOMO_PLATFORM_GBA) || defined(DOOMO_PLATFORM_PS1)
#define DOOMO_TRIGGERS 0
#else
#define DOOMO_TRIGGERS 1
#endif
void     input_set_caps(unsigned caps, int ltrig, int rtrig);
unsigned input_caps(void);                     /* 0 when no analog pad, else | PRESENT */
int      input_triggers(int *l, int *r);       /* 1 = analog triggers (0..255) */
/* Radial dead zone (STICK_DEAD of 128) -> -256..256 per axis; returns the
 * magnitude 0..256 (0 inside the dead zone). stick_curve(): the response
 * curve (gentle near the centre, linear at the rim), -256..256. */
#define STICK_DEAD 22
int  stick_dz(int x, int y, int *ox, int *oy);
int  stick_curve(int v);
/* The sticks dead-zoned (both 0 and returns 0 with no analog pad or inside
 * the dead zone); the right stick is 0 on a pad without one. */
int  input_lstick(int *x, int *y);
int  input_rstick(int *x, int *y);
/* Right stick X as a button: -1 / +1 the tick it is pushed past half way,
 * then auto-repeat like btn_repeat() while held; 0 otherwise. For cameras
 * that turn in steps (the house's 90-degree view). */
int  input_rflick(void);
/* The D-pad bits a stick at (lx, ly) presses for menus (48 of 128). */
uint32_t input_stick_dpad(int lx, int ly);
/* "Smooth feel": eased turning, fire buffering, aim friction. Off by default;
 * the PS1 port turns it on. */
void input_set_feel(int on);
int  input_feel(void);

/* ---- foundation additions ---- */
void input_reset(void);              /* forget all state (tests, boot)   */

/* ---- C3: the pad in the player's hands, for the names on screen ----------
 * input_pad() says which pad the button names must fit: the console's own
 * (compile-time DOOMO_PLATFORM_*), on the native runner PAD_GBA while the
 * platform sets DOOMO_INPUT_SYNTH_XY (--gba-input) and PAD_HOST otherwise.
 * input_set_pad() forces one (tests; -1 = automatic again).
 * btn_name() is the name of ONE button (the lowest bit of b) on that pad:
 *   GBA:  X = "SEL+A", Y = "SEL+B", SELECT = "SEL"; no L2 / R2 (NULL)
 *   PS1:  L = "L1", R = "R1", X = "Square", Y = "Triangle", L2, R2
 *   DC:   L / R are the triggers, SELECT = "START+Y" (dc/src/main.c); no L2 / R2
 *   host: A B X Y L R L2 R2 START SEL
 * A and B are drawn as button caps by the UI (STR_ICON_A / STR_ICON_B), the
 * D-pad as arrows; btn_name() still names them ("A", "Up"). NULL = this pad
 * has no such button: say nothing about it on screen. */
enum { PAD_HOST, PAD_GBA, PAD_PS1, PAD_DC, PAD_COUNT };
int  input_pad(void);
void input_set_pad(int pad);
const char *btn_name(uint32_t b);

#endif
