/* pad.c - controller, analog stick and keyboard through Maple (DC4;
 * owner: dreamcast). The mapping itself is portable (padmap.c, dcinput.h).
 *
 * Every tick, before doomo_frame():
 *   - the first controller Maple found (any port) gives the buttons, the
 *     analog stick (input_set_analog, as the PS1's DualShock) and the
 *     triggers; the core's "smooth feel" (eased turning, aim friction) is
 *     on, as on the PS1
 *   - every keyboard adds its buttons (dcinput.h); while the on-screen
 *     keyboard is open (SCENE_TEXT_ENTRY) the characters typed go straight
 *     into it (ui_text_entry_feed, the native runner's `type` path) and
 *     only the keys that type nothing act as buttons
 * KallistiOS polls Maple once a vblank, so a tick sees what the last poll
 * saw. A keyboard's key queue keeps every key typed between two ticks.
 */
#include <kos.h>
#include <dc/maple.h>
#include <dc/maple/controller.h>
#include <dc/maple/keyboard.h>
#include "dc_plat.h"
#include "dcinput.h"
#include "doomo/input.h"
#include "doomo/scene.h"
#include "doomo/scenes.h"
#include "doomo/ui.h"
#include "doomo/plat_ext.h"

static int kbd_seen;

int dc_keyboard_present(void) { return kbd_seen; }

void dc_pad_init(void)
{
    input_set_feel(1);                  /* the PS1's analog feel: the Dreamcast pad has a stick */
}

static uint32_t read_controller(void)
{
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *st;
    DcPadRaw raw;
    DcPadOut out;
    if (!dev || !(st = (cont_state_t *)maple_dev_status(dev))) {
        input_set_analog(0, 0, 0, 0, 0, 0);
        return 0;
    }
    raw.buttons = st->buttons;
    raw.ltrig = st->ltrig;
    raw.rtrig = st->rtrig;
    raw.joyx = st->joyx;
    raw.joyy = st->joyy;
    raw.joy2x = st->joy2x;
    raw.joy2y = st->joy2y;
    raw.analog = (dev->info.function_data[0] & (CONT_CAPABILITY_ANALOG_X | CONT_CAPABILITY_ANALOG_Y)) != 0;
    raw.analog2 = (dev->info.function_data[0] & (CONT_CAPABILITY_ANALOG2_X | CONT_CAPABILITY_ANALOG2_Y)) != 0;
    raw.triggers = (dev->info.function_data[0] & (CONT_CAPABILITY_LTRIG | CONT_CAPABILITY_RTRIG)) != 0;
    dc_pad_map(&raw, &out);
    input_set_analog(out.analog, out.lx, out.ly, out.rx, out.ry, out.stick_mask);
    input_set_caps(out.caps, out.ltrig, out.rtrig);             /* ST1 */
    return out.btn;
}

static uint32_t read_keyboards(void)
{
    uint32_t b = 0;
    int n, open = scene_top() == &SCENE_TEXT_ENTRY, seen = 0;
    for (n = 0; ; n++) {
        maple_device_t *dev = maple_enum_type(n, MAPLE_FUNC_KEYBOARD);
        kbd_state_t *st;
        uint8_t down[KBD_MAX_KEYS];
        int k, v;
        if (!dev) break;
        seen = 1;
        /* typed characters: into the open text entry, else dropped */
        while ((v = kbd_queue_pop(dev, true)) != KBD_QUEUE_END) {
            int c = dc_kbd_text_char(v);
            if (open && c >= 0) ui_text_entry_feed(c);
        }
        if (!(st = kbd_get_state(dev))) continue;
        for (k = 0; k < KBD_MAX_KEYS; k++) down[k] = st->key_states[k].is_down;
        b |= dc_kbd_buttons(down, KBD_MAX_KEYS, open);
    }
    kbd_seen = seen;
    return b;
}

uint32_t dc_read_pad(void)
{
    return read_controller() | read_keyboards();
}

/* What the core may count on (plat_ext.h): the machine config record on
 * the VMU (vmusave.c) always; the rumble pack and a keyboard while they
 * are plugged in. */
uint32_t plat_features(void)
{
    uint32_t f = PLATF_CONFIG;
    if (dc_rumble_present()) f |= PLATF_RUMBLE;
    if (dc_keyboard_present()) f |= PLATF_KEYBOARD;
    return f;
}
