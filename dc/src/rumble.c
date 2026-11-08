/* rumble.c - Dreamcast Puru Puru (jump pack) support (X2; owner: dreamcast).
 *
 * plat_rumble() is the core's hook (plat_ext.h): camfx_shake() asks for a
 * pulse as strong and as long as the shake. The pack in controller 1's
 * first expansion slot is used; none attached = nothing happens (the call
 * is still logged so the Flycast harness can `expect-log` it).
 * plat_features() reports PLATF_RUMBLE only while a pack is plugged in, so
 * rows and hints that depend on a motor follow the hardware.
 *
 * The KOS effect word has an intensity of 1..7 per motor and a duration
 * byte; both are derived from the core's 0..255 strength and tick count.
 * A new pulse replaces the one running.
 */
#include <kos.h>
#include <dc/maple.h>
#include <dc/maple/purupuru.h>
#include "dc_plat.h"
#include "doomo/plat_ext.h"

#ifndef PURUPURU_EFFECT1_INTENSITY
#define PURUPURU_EFFECT1_INTENSITY(x) ((x) & 0x07)
#endif
#ifndef PURUPURU_SPECIAL_MOTOR1
#define PURUPURU_SPECIAL_MOTOR1 0x10
#endif

static maple_device_t *pack(void)
{
    return maple_enum_type(0, MAPLE_FUNC_PURUPURU);
}

uint32_t plat_features(void)
{
    return pack() ? PLATF_RUMBLE : 0u;
}

void plat_rumble(int strength, int ticks)
{
    maple_device_t *dev;
    purupuru_effect_t fx;
    int level, dur;
    if (strength <= 0 || ticks <= 0)
        return;
    if (strength > 255) strength = 255;
    level = (strength * 7 + 254) / 255;          /* 1..7 */
    dur = ticks * 4;                             /* ~1/120 s units, clamped below */
    if (dur < 6) dur = 6;
    if (dur > 255) dur = 255;
    dc_logf("DC: rumble strength %d ticks %d (level %d)", strength, ticks, level);
    dev = pack();
    if (!dev)
        return;
    /* the same four bytes as the old named fields (special, effect1,
     * effect2, duration), written through `raw`: KOS at the pinned commit
     * marks those names deprecated and the build uses -Werror */
    fx.raw = (uint32_t)PURUPURU_SPECIAL_MOTOR1 |
             ((uint32_t)PURUPURU_EFFECT1_INTENSITY(level) << 8) |
             ((uint32_t)dur << 24);
    purupuru_rumble(dev, &fx);
}
