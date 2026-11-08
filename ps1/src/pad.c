/* pad.c - controller input through the BIOS pad driver (updated every
 * vblank). Digital pads, Dual Analog and DualShock (the left stick doubles
 * as the D-pad). Mapping (input.h): Cross=A, Circle=B, Square=X,
 * Triangle=Y, L1=L, R1=R, L2, R2, START, SELECT, L3, R3.
 *
 * (ST1) Auto analog: a DualShock powers up digital (green-less LED) and
 * the BIOS driver only ever polls it, so the sticks did nothing until the
 * player found the ANALOG button. When a pad shows up (boot, or plugged in
 * / swapped later) and reads as digital, pad_read() sends it the DualShock
 * config sequence over SIO0 itself - enter config (43h), analog mode +
 * lock (44h 01 03), exit config (43h 00) - between two BIOS polls with
 * interrupts off, a few times at most. A plain digital pad ignores it
 * (it answers 43h as a poll) and stays digital; a Dual Analog / DualShock
 * comes up red-LED analog with the ANALOG button locked. The BIOS driver
 * keeps polling as before (it sets SIO0 up on every poll).
 */
#include <psxapi.h>
#include <psxpad.h>
#include <hwregs_c.h>
#include "ps1.h"

static uint8_t s_padbuf[2][34];

void pad_init(void)
{
    /* StartPAD() is called by memcard_init() after InitCARD() (the BIOS
     * wants InitPAD -> InitCARD -> StartPAD -> StartCARD) */
    InitPAD(s_padbuf[0], 34, s_padbuf[1], 34);
}

static int s_lx = 128, s_ly = 128, s_rx = 128, s_ry = 128, s_analog;
static uint32_t s_stick_mask;
/* the harness (run.py `stick`, written straight into RAM): sticks forced, on any pad */
volatile int32_t g_pad_force[5];
/* auto analog: last frame 0 no pad / 1 digital / 2 analog, config tries left, frames to wait */
static uint8_t s_conn, s_tries, s_wait;

int pad_analog(int *lx, int *ly, int *rx, int *ry)
{
    *lx = s_lx - 128; *ly = s_ly - 128; *rx = s_rx - 128; *ry = s_ry - 128;
    return s_analog;
}

/* BTN_ bits the last pad_read() synthesised from the left stick. */
uint32_t pad_stick_mask(void) { return s_stick_mask; }

/* ---- the DualShock config sequence (ST1) ------------------------------------ */
static void sio_wait(int n) { volatile int i; for (i = 0; i < n; i++) { } }

static void sio_cmd(const uint8_t *cmd, int n)
{
    int i, t;
    EnterCriticalSection();
    SIO_CTRL(0) = 0x0013;                          /* TX on, /JOY1 low (port 1), ack reset */
    sio_wait(300);                                  /* the pad wakes up (~20 us) */
    for (i = 0; i < n; i++) {
        for (t = 0; t < 4000 && !(SIO_STAT(0) & 1); t++) { }   /* TX ready */
        SIO_DATA(0) = cmd[i];
        for (t = 0; t < 4000 && !(SIO_STAT(0) & 2); t++) { }   /* the reply byte */
        (void)SIO_DATA(0);
        SIO_CTRL(0) = 0x0013;                       /* acknowledge */
        sio_wait(400);                              /* the pad's /ACK (~10-100 us) */
    }
    SIO_CTRL(0) = 0;                                /* deselect */
    ExitCriticalSection();
    sio_wait(2000);                                 /* a gap between commands */
}

static void __attribute__((noinline)) pad_make_analog(void)
{
    static const uint8_t enter[9] = { 0x01, 0x43, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 };
    static const uint8_t mode[9]  = { 0x01, 0x44, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00 };
    static const uint8_t leave[9] = { 0x01, 0x43, 0x00, 0x00, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A };
    uint16_t m = SIO_MODE(0), b = SIO_BAUD(0);
    SIO_MODE(0) = 0x000D;                           /* 8 bits, /1 */
    SIO_BAUD(0) = 0x0088;                           /* 250 kHz, as the BIOS */
    sio_cmd(enter, 9);
    sio_cmd(mode, 9);
    sio_cmd(leave, 9);
    SIO_MODE(0) = m;
    SIO_BAUD(0) = b;
}

uint32_t pad_read(void)
{
    const PADTYPE *p = (const PADTYPE *)s_padbuf[0];
    uint32_t out = 0;
    uint16_t b;
    int conn = p->stat == 0 && (p->type == PAD_ID_DIGITAL || p->type == PAD_ID_ANALOG ||
                                p->type == PAD_ID_ANALOG_STICK);
    s_analog = 0;
    s_stick_mask = 0;
    s_lx = s_ly = s_rx = s_ry = 128;
    /* auto analog: a pad just showed up - wait a few frames, then up to 3 tries */
    if (conn && !s_conn) { s_tries = 3; s_wait = 8; }
    if (conn && p->type != PAD_ID_DIGITAL && s_conn != 2) dbg("pad: analog\n");
    s_conn = (uint8_t)(conn ? 1 + (p->type != PAD_ID_DIGITAL) : 0);
    if (conn && p->type == PAD_ID_DIGITAL && s_tries && --s_wait == 0) {
        pad_make_analog();
        s_tries--;
        s_wait = 20;
    }
    if (conn) {
        /* the pad's bits (psxpad.h PAD_*, bit i) -> BTN_*; a digital pad reads L3 / R3 up */
        static const uint16_t map[16] = {
            BTN_SELECT, BTN_L3, BTN_R3, BTN_START, BTN_UP, BTN_RIGHT, BTN_DOWN, BTN_LEFT,
            BTN_L2, BTN_R2, BTN_L, BTN_R, BTN_Y, BTN_B, BTN_A, BTN_X
        };
        int i;
        b = (uint16_t)~p->btn;
        for (i = 0; i < 16; i++)
            if (b & (1u << i)) out |= map[i];
        if (p->type == PAD_ID_ANALOG || p->type == PAD_ID_ANALOG_STICK) {
            s_analog = 1;
            s_lx = p->ls_x; s_ly = p->ls_y; s_rx = p->rs_x; s_ry = p->rs_y;
        }
    }
    if (g_pad_force[0]) {
        s_analog = 1;
        s_lx = 128 + g_pad_force[1]; s_ly = 128 + g_pad_force[2];
        s_rx = 128 + g_pad_force[3]; s_ry = 128 + g_pad_force[4];
    }
    if (s_analog) {
        if (s_lx < 128 - 48) s_stick_mask |= BTN_LEFT;
        if (s_lx > 128 + 48) s_stick_mask |= BTN_RIGHT;
        if (s_ly < 128 - 48) s_stick_mask |= BTN_UP;
        if (s_ly > 128 + 48) s_stick_mask |= BTN_DOWN;
        out |= s_stick_mask;
    }
    return out;
}
