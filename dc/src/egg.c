/* egg.c - the Dreamcast boot easter egg (X8; owner: dreamcast).
 *
 * Hold A + B + X + Y + START on controller 1 while the console boots and
 * the game opens with a one-second rainbow: the PVR background steps
 * through the pastel/hellfire palette in time with the vblank, then the
 * normal boot carries on. Nothing is saved or changed, and without the
 * buttons the cost is a short wait for the controller to appear (the maple
 * bus needs a moment after power-on), capped at a second and a half.
 *
 * It uses only the PVR background colour and empty scenes, so it works
 * before the core, the 2D layer (DC3) or any asset is up.
 */
#include <kos.h>
#include <dc/pvr.h>
#include <dc/maple.h>
#include <dc/maple/controller.h>
#include "dc_plat.h"

#define EGG_MASK (CONT_A | CONT_B | CONT_X | CONT_Y | CONT_START)
#define WAIT_MAX 90                 /* vblanks to wait for the pad */
#define EGG_FRAMES 60               /* one second at 60 Hz         */

static const uint8_t k_rainbow[8][3] = {
    { 255, 120, 170 }, { 255, 170, 90 }, { 255, 230, 110 }, { 140, 230, 130 },
    { 110, 210, 240 }, { 150, 140, 250 }, { 230, 90, 220 }, { 220, 40, 50 }
};

static int egg_held(void)
{
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *st;
    if (!dev)
        return 0;
    st = (cont_state_t *)maple_dev_status(dev);
    return st && (st->buttons & EGG_MASK) == EGG_MASK;
}

void dc_boot_egg(void)
{
    int i, held = 0;
    for (i = 0; i < WAIT_MAX; i++) {
        if (maple_enum_type(0, MAPLE_FUNC_CONTROLLER)) {
            held = egg_held();
            break;
        }
        vid_waitvbl();
    }
    if (!held)
        return;
    dc_log("DC: boot easter egg");
    for (i = 0; i < EGG_FRAMES; i++) {
        const uint8_t *c = k_rainbow[(i / 4) & 7];
        pvr_set_bg_color(c[0] / 255.0f, c[1] / 255.0f, c[2] / 255.0f);
        pvr_wait_ready();
        pvr_scene_begin();
        pvr_scene_finish();
    }
}
