/* dcinput.h - Dreamcast controls (DC4; owner: dreamcast).
 *
 * padmap.c is portable C (no KallistiOS): the host test (dc/tests/vmu)
 * checks every mapping. pad.c reads Maple through KallistiOS and uses it.
 *
 * Controller (also arcade sticks and twin sticks):
 *   D-pad, A, B, X, Y      as themselves
 *   L / R trigger          L / R when pressed past half way (> 127 of 255)
 *   START                  START; START+Y is SELECT (DECISIONS.md: the pad
 *                          has no SELECT), and then neither START nor Y
 *   C / Z (6-button pads,  L / R (they have no triggers)
 *   arcade sticks)
 *   D                      SELECT
 *   second D-pad (twin     left = L2, right = R2 (the PS1's shoulder pair;
 *   stick) left / right    a standard pad has none, as on the GBA)
 *   analog stick           input_set_analog() every tick (-128..127, +y
 *                          down) and the D-pad bits past 48 of 128 (the
 *                          PS1's threshold), so menus move with it too
 *   second analog stick    input_set_analog()'s rx / ry when there is one
 *   (ST1) analog L / R     input_set_caps(INPUT_CAP_TRIGGERS, l, r): with the
 *                          default "Auto" controls the shooter plays the
 *                          one-stick scheme (stick X turns, Y walks, the
 *                          triggers strafe by pressure, both = lock on)
 * Keyboard (any port): see dc_kbd_buttons(); while the on-screen keyboard
 * (SCENE_TEXT_ENTRY) is open the typed characters go straight into it.
 */
#ifndef DC_DCINPUT_H
#define DC_DCINPUT_H

#include <stdint.h>

/* KallistiOS' controller bits (dc/maple/controller.h), repeated so the
 * mapping builds on the host */
enum {
    DCP_C = 1u << 0, DCP_B = 1u << 1, DCP_A = 1u << 2, DCP_START = 1u << 3,
    DCP_UP = 1u << 4, DCP_DOWN = 1u << 5, DCP_LEFT = 1u << 6, DCP_RIGHT = 1u << 7,
    DCP_Z = 1u << 8, DCP_Y = 1u << 9, DCP_X = 1u << 10, DCP_D = 1u << 11,
    DCP_UP2 = 1u << 12, DCP_DOWN2 = 1u << 13, DCP_LEFT2 = 1u << 14, DCP_RIGHT2 = 1u << 15
};

typedef struct DcPadRaw {
    uint32_t buttons;            /* DCP_* held                                   */
    int      ltrig, rtrig;       /* 0..255                                       */
    int      joyx, joyy;         /* -128..127, 0 = centre                        */
    int      joy2x, joy2y;
    int      analog, analog2;    /* the device has the stick / a second stick    */
    int      triggers;           /* (ST1) the device has analog L / R triggers   */
} DcPadRaw;

typedef struct DcPadOut {
    uint32_t btn;                /* BTN_* for doomo_frame                        */
    uint32_t stick_mask;         /* the BTN_ D-pad bits that came from the stick */
    int      analog;             /* input_set_analog(present, ...)               */
    int      lx, ly, rx, ry;
    unsigned caps;               /* (ST1) input_set_caps(): INPUT_CAP_*          */
    int      ltrig, rtrig;       /* (ST1) the triggers, 0..255                   */
} DcPadOut;

#define DC_STICK_DIGITAL 48      /* stick deflection that also presses the D-pad */
#define DC_TRIGGER_ON    127     /* trigger value above which L / R are down     */

void dc_pad_map(const DcPadRaw *raw, DcPadOut *out);

/* Keyboard (USB HID usage codes, which the Dreamcast keyboard uses too):
 *   arrows                 D-pad
 *   Z, Space, Enter        A                 X, Escape, Backspace   B
 *   C                      X                 V                      Y
 *   Q / W                  L / R             1 / 2                  L2 / R2
 *   P, F1                  START             Tab                    SELECT
 * keys[k] is nonzero while key code k is down (k < 256). text_open: the
 * on-screen keyboard is open, so only the keys that type nothing (arrows,
 * Escape, Tab, F1) act as buttons. */
uint32_t dc_kbd_buttons(const uint8_t *keys, int nkeys, int text_open);
/* A value from KallistiOS' kbd_queue_pop(dev, true) as a character for
 * ui_text_entry_feed(): printable ASCII, '\n' for Enter, '\b' for
 * Backspace; -1 for keys that type nothing here. */
int dc_kbd_text_char(int popped);

#endif
