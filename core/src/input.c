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

#define NBTN 14
#define REPEAT_DELAY 12
#define REPEAT_RATE 3

static uint32_t cur, prev;
static uint32_t ignore;          /* held through input_eat(): wait for release */
static uint32_t held[NBTN];
static uint8_t  eaten;

void input_update(uint32_t raw)
{
    uint32_t b = raw & ((1u << NBTN) - 1);
    int i;
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
}

int btn_down(uint32_t b) { return !eaten && (cur & b) != 0; }

int btn_pressed(uint32_t b) { return !eaten && (cur & ~prev & b) != 0; }

int btn_released(uint32_t b) { return !eaten && (~cur & prev & b) != 0; }

int btn_repeat(uint32_t b)
{
    int i;
    if (eaten) return 0;
    for (i = 0; i < NBTN; i++) {
        uint32_t h;
        if (!(b & (1u << i))) continue;
        h = held[i];
        if (h == 1) return 1;
        if (h > REPEAT_DELAY && (h - REPEAT_DELAY - 1) % REPEAT_RATE == 0) return 1;
    }
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
    ignore |= cur;               /* buttons held now must be released first */
    cur = prev = 0;
    memset(held, 0, sizeof(held));
}

static int a_present, a_lx, a_ly, a_rx, a_ry, feel_on;
static uint32_t a_mask;

void input_set_analog(int present, int lx, int ly, int rx, int ry, uint32_t stick_mask)
{
    a_present = present; a_lx = lx; a_ly = ly; a_rx = rx; a_ry = ry; a_mask = stick_mask;
}

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
    memset(held, 0, sizeof(held));
}
