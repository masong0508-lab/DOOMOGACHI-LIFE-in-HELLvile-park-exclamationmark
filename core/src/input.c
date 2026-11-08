/* input.c - edge detection, hold timers and auto-repeat (owner:
 * foundation).
 *
 * Auto-repeat: btn_repeat() fires on the press tick, then once the button
 * has been held 12 ticks it fires again every 3 ticks.
 *
 * GBA (DOOMO_INPUT_SYNTH_XY): while SELECT is held, A reads as BTN_X and
 * B reads as BTN_Y (A and B themselves are suppressed); SELECT on its own
 * still reports normally.
 */
#include "doomo/input.h"
#include "doomo/fixmath.h"

#define NBTN (DOOMO_STICKS ? 16 : 14)  /* L3 / R3: not on the GBA */
#define REPEAT_DELAY 12
#define REPEAT_RATE 3

static uint32_t cur, prev;
static uint32_t ignore;          /* held through input_eat(): wait for release */
static uint32_t held[NBTN];
static uint8_t  eaten;
static uint8_t  synth_seen;
static void     flick_update(void);
static uint16_t fl_held;         /* ST1: ticks the right stick X has been past half */
static int8_t   fl_dir, fl_wait; /* -1 / 0 / +1; 1 = wait for the stick to centre (input_eat) */

/* btn_repeat()'s rhythm for a hold of h ticks */
static int rep_tick(uint32_t h)
{
    return h == 1 || (h > REPEAT_DELAY && (h - REPEAT_DELAY - 1) % REPEAT_RATE == 0);
}      /* C3: the last raw mask had DOOMO_INPUT_SYNTH_XY */
static int8_t   pad_forced = -1; /* C3: input_set_pad() */

void input_update(uint32_t raw)
{
    uint32_t b = raw & ((1u << NBTN) - 1);
    int i;
    synth_seen = (raw & DOOMO_INPUT_SYNTH_XY) != 0;
    if (raw & DOOMO_INPUT_SYNTH_XY) {
        b &= ~(uint32_t)(BTN_X | BTN_Y);
        if (b & BTN_SELECT) {
            if (b & BTN_A) { b &= ~(uint32_t)BTN_A; b |= BTN_X; }
            if (b & BTN_B) { b &= ~(uint32_t)BTN_B; b |= BTN_Y; }
        }
    }
    ignore &= b;                 /* released buttons count again */
    b &= ~ignore;
    prev = cur;
    cur = b;
    eaten = 0;
    for (i = 0; i < NBTN; i++) {
        if (cur & (1u << i)) { if (held[i] < 0xFFFFFFF0u) held[i]++; }
        else held[i] = 0;
    }
    if (DOOMO_STICKS) flick_update();
}

int btn_down(uint32_t b) { return !eaten && (cur & b) != 0; }

int btn_pressed(uint32_t b) { return !eaten && (cur & ~prev & b) != 0; }

int btn_released(uint32_t b) { return !eaten && (~cur & prev & b) != 0; }

int btn_repeat(uint32_t b)
{
    int i;
    if (eaten) return 0;
    for (i = 0; i < NBTN; i++)
        if ((b & (1u << i)) && rep_tick(held[i])) return 1;
    return 0;
}

uint32_t btn_held_ticks(uint32_t b)
{
    int i;
    for (i = 0; i < NBTN; i++)
        if (b & (1u << i)) return held[i];
    return 0;
}

void input_eat(void)
{
    eaten = 1;
    if (DOOMO_STICKS && fl_held) fl_wait = 1;   /* a held flick waits for the stick to centre */
    ignore |= cur;               /* buttons held now must be released first */
    cur = prev = 0;
    memset(held, 0, sizeof(held));
}

static int a_present, a_lx, a_ly, a_rx, a_ry, feel_on, a_lt, a_rt;
static unsigned a_caps;
static uint32_t a_mask;

void input_set_analog(int present, int lx, int ly, int rx, int ry, uint32_t stick_mask)
{
    a_present = present; a_lx = lx; a_ly = ly; a_rx = rx; a_ry = ry; a_mask = stick_mask;
    if (DOOMO_STICKS) { a_caps = INPUT_CAP_RSTICK; a_lt = a_rt = 0; }
}

void input_set_caps(unsigned caps, int ltrig, int rtrig)
{
    a_caps = caps;
    a_lt = DOOMO_CLAMP(ltrig, 0, 255);
    a_rt = DOOMO_CLAMP(rtrig, 0, 255);
}

unsigned input_caps(void) { return DOOMO_STICKS && a_present ? a_caps | INPUT_CAP_PRESENT : 0; }

int input_triggers(int *l, int *r)
{
    int on = DOOMO_TRIGGERS && a_present && (a_caps & INPUT_CAP_TRIGGERS);
    *l = on ? a_lt : 0;
    *r = on ? a_rt : 0;
    return on;
}

int stick_dz(int x, int y, int *ox, int *oy)
{
    int m = (int)fx_dist(INT2FIX(x), INT2FIX(y)) >> 16, out;
    *ox = *oy = 0;
    if (m <= STICK_DEAD) return 0;
    out = (m - STICK_DEAD) * 256 / (125 - STICK_DEAD);      /* 0..256 */
    if (out > 256) out = 256;
    *ox = x * out / m;                              /* |x| <= m: within -256..256 */
    *oy = y * out / m;
    return out;
}

int stick_curve(int v)
{
    int a = v < 0 ? -v : v;
    int c = (a * a / 256 * 2 + a) / 3;
    return v < 0 ? -c : c;
}

int input_lstick(int *x, int *y)
{
    *x = *y = 0;
    return DOOMO_STICKS && a_present ? stick_dz(a_lx, a_ly, x, y) : 0;
}

int input_rstick(int *x, int *y)
{
    *x = *y = 0;
    return DOOMO_STICKS && a_present && (a_caps & INPUT_CAP_RSTICK) ? stick_dz(a_rx, a_ry, x, y) : 0;
}

uint32_t input_stick_dpad(int lx, int ly)
{
    uint32_t s = 0;
    if (lx < -48) s |= BTN_LEFT;
    if (lx > 48)  s |= BTN_RIGHT;
    if (ly < -48) s |= BTN_UP;
    if (ly > 48)  s |= BTN_DOWN;
    return s;
}

/* once per tick (input_update): the right stick's flick, held */
static void flick_update(void)
{
    int x, y, d;
    input_rstick(&x, &y);
    d = x > 128 ? 1 : x < -128 ? -1 : 0;
    if (fl_wait) { if (x > -64 && x < 64) fl_wait = 0; d = 0; }
    fl_held = (uint16_t)(d && d == fl_dir ? (fl_held < 0xFFF0 ? fl_held + 1 : fl_held) : d != 0);
    fl_dir = (int8_t)d;
}

int input_rflick(void) { return DOOMO_STICKS && !eaten && rep_tick(fl_held) ? fl_dir : 0; }


int input_analog(int *lx, int *ly, int *rx, int *ry)
{
    *lx = a_lx; *ly = a_ly; *rx = a_rx; *ry = a_ry;
    return a_present;
}

uint32_t input_stick_mask(void) { return a_present ? a_mask : 0; }
void input_set_feel(int on) { feel_on = on; }
int  input_feel(void) { return feel_on; }

/* For tests: forget all state. */
void input_reset(void)
{
    cur = prev = ignore = 0;
    eaten = 0;
    if (DOOMO_STICKS) { fl_held = 0; fl_dir = fl_wait = 0; }
    memset(held, 0, sizeof(held));
}

/* ---- C3: button names per pad (input.h btn_name) ------------------------ */
int input_pad(void)
{
    if (pad_forced >= 0) return pad_forced;
#if defined(DOOMO_PLATFORM_GBA)
    return PAD_GBA;
#elif defined(DOOMO_PLATFORM_PS1)
    return PAD_PS1;
#elif defined(DOOMO_PLATFORM_DC)
    return PAD_DC;
#else
    return synth_seen ? PAD_GBA : PAD_HOST;
#endif
}

void input_set_pad(int pad)
{
    pad_forced = (int8_t)(pad >= 0 && pad < PAD_COUNT ? pad : -1);
}

/* in BTN_* bit order: UP DOWN LEFT RIGHT A B L R START SELECT X Y L2 R2 L3 R3 */
static const char *const pad_names[PAD_COUNT][16] = {
    { "Up", "Down", "Left", "Right", "A", "B", "L", "R", "START", "SEL", "X", "Y", "L2", "R2", "L3", "R3" },
    { "Up", "Down", "Left", "Right", "A", "B", "L", "R", "START", "SEL", "SEL+A", "SEL+B", 0, 0, 0, 0 },
    { "Up", "Down", "Left", "Right", "A", "B", "L1", "R1", "START", "SEL", "Square", "Triangle", "L2", "R2", "L3", "R3" },
    { "Up", "Down", "Left", "Right", "A", "B", "L", "R", "START", "START+Y", "X", "Y", 0, 0, 0, 0 },
};

const char *btn_name(uint32_t b)
{
    int i;
    for (i = 0; i < NBTN; i++)
        if (b & (1u << i)) return pad_names[input_pad()][i];
    return 0;
}
