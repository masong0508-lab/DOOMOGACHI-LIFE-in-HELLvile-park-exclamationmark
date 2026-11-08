/* padmap.c - Dreamcast controller and keyboard to the core's buttons
 * (DC4; owner: dreamcast). Portable C; the mapping is in dcinput.h. */
#include "dcinput.h"
#include "doomo/input.h"

static int clamp_axis(int v) { return v < -128 ? -128 : v > 127 ? 127 : v; }

void dc_pad_map(const DcPadRaw *r, DcPadOut *o)
{
    uint32_t k = r->buttons, b = 0, s = 0;
    if (k & DCP_UP)    b |= BTN_UP;
    if (k & DCP_DOWN)  b |= BTN_DOWN;
    if (k & DCP_LEFT)  b |= BTN_LEFT;
    if (k & DCP_RIGHT) b |= BTN_RIGHT;
    if (k & DCP_A)     b |= BTN_A;
    if (k & DCP_B)     b |= BTN_B;
    if (k & DCP_X)     b |= BTN_X;
    if (k & DCP_Y)     b |= BTN_Y;
    if (r->ltrig > DC_TRIGGER_ON || (k & DCP_C)) b |= BTN_L;
    if (r->rtrig > DC_TRIGGER_ON || (k & DCP_Z)) b |= BTN_R;
    if (k & DCP_LEFT2)  b |= BTN_L2;
    if (k & DCP_RIGHT2) b |= BTN_R2;
    if (k & DCP_D)      b |= BTN_SELECT;
    /* the pad has no SELECT: START+Y is SELECT (DECISIONS.md) */
    if ((k & DCP_START) && (k & DCP_Y)) b = (b & ~(uint32_t)BTN_Y) | BTN_SELECT;
    else if (k & DCP_START) b |= BTN_START;

    o->analog = r->analog != 0;
    o->lx = o->ly = o->rx = o->ry = 0;
    if (o->analog) {
        o->lx = clamp_axis(r->joyx);
        o->ly = clamp_axis(r->joyy);
        if (o->lx < -DC_STICK_DIGITAL) s |= BTN_LEFT;
        if (o->lx > DC_STICK_DIGITAL)  s |= BTN_RIGHT;
        if (o->ly < -DC_STICK_DIGITAL) s |= BTN_UP;
        if (o->ly > DC_STICK_DIGITAL)  s |= BTN_DOWN;
        if (r->analog2) {
            o->rx = clamp_axis(r->joy2x);
            o->ry = clamp_axis(r->joy2y);
        }
    }
    o->stick_mask = s;
    o->btn = b | s;
}

/* HID usage codes */
enum {
    K_C = 0x06, K_P = 0x13, K_Q = 0x14, K_V = 0x19, K_W = 0x1A,
    K_X = 0x1B, K_Z = 0x1D, K_1 = 0x1E, K_2 = 0x1F, K_ENTER = 0x28, K_ESC = 0x29,
    K_BKSP = 0x2A, K_TAB = 0x2B, K_SPACE = 0x2C, K_F1 = 0x3A, K_RIGHT = 0x4F, K_LEFT = 0x50,
    K_DOWN = 0x51, K_UP = 0x52, K_PAD_ENTER = 0x58
};

uint32_t dc_kbd_buttons(const uint8_t *keys, int n, int text_open)
{
    static const struct { uint8_t key; uint8_t types; uint32_t btn; } map[] = {
        { K_UP, 0, BTN_UP }, { K_DOWN, 0, BTN_DOWN }, { K_LEFT, 0, BTN_LEFT }, { K_RIGHT, 0, BTN_RIGHT },
        { K_Z, 1, BTN_A }, { K_SPACE, 1, BTN_A }, { K_ENTER, 1, BTN_A }, { K_PAD_ENTER, 1, BTN_A },
        { K_X, 1, BTN_B }, { K_ESC, 0, BTN_B }, { K_BKSP, 1, BTN_B },
        { K_C, 1, BTN_X }, { K_V, 1, BTN_Y }, { K_Q, 1, BTN_L }, { K_W, 1, BTN_R },
        { K_1, 1, BTN_L2 }, { K_2, 1, BTN_R2 },
        { K_P, 1, BTN_START }, { K_F1, 0, BTN_START }, { K_TAB, 0, BTN_SELECT },
    };
    uint32_t b = 0;
    unsigned i;
    if (!keys) return 0;
    for (i = 0; i < sizeof(map) / sizeof(map[0]); i++)
        if (map[i].key < n && keys[map[i].key] && !(text_open && map[i].types))
            b |= map[i].btn;
    return b;
}

int dc_kbd_text_char(int v)
{
    if (v == 13 || v == 10) return '\n';
    if (v == 8) return '\b';
    if (v >= 0x20 && v <= 0x7E) return v;
    if (v < 0 && v >= -128) return v + 256;       /* ISO-8859-1 above 0x7F (signed char) */
    return -1;                                     /* raw key << 8, Escape, Tab, ...      */
}
