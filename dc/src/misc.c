/* misc.c - the harness mailbox, logging and the misc platform calls
 * (owner: dreamcast).
 *
 * plat_log() lines go two ways: to dbgio (the serial port on hardware, the
 * console in an emulator) and into the mailbox's log ring, which the Flycast
 * harness reads from RAM (dc_plat.h). The core announces each scene change
 * as "scene: set NAME" / "scene: push NAME" / "scene: pop NAME"; the
 * mailbox keeps the name of the scene that is on top in dc_mailbox.scene so
 * the harness's expect-scene needs no log parsing.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <kos/dbgio.h>
#include <arch/timer.h>
#include "dc_plat.h"
#include "doomo/scene.h"

/* in .bss like everything else; the harness finds it by symbol */
__attribute__((used, aligned(32))) volatile DcMailbox dc_mailbox;
/* sh-elf-nm prints C symbols with the SH ABI's '_' prefix (_dc_mailbox);
 * this alias makes the plain name `dc_mailbox` appear in the symbol table
 * too, for tools that look it up as the C name */
__asm__(".global dc_mailbox\n.set dc_mailbox, _dc_mailbox");

static void ring_put(const char *s)
{
    uint32_t h = dc_mailbox.log_wr;
    for (; *s; s++, h++) dc_mailbox.log[h & (DC_MB_LOG_SIZE - 1)] = *s;
    dc_mailbox.log[h & (DC_MB_LOG_SIZE - 1)] = '\n';
    h++;
    dc_mailbox.log_wr = h;
}

static void scene_name(const char *nm)
{
    size_t i;
    for (i = 0; i < sizeof(dc_mailbox.scene) - 1 && nm[i]; i++)
        dc_mailbox.scene[i] = nm[i];
    dc_mailbox.scene[i] = 0;
}

/* "scene: set X" / "scene: push X" name the new top; after "scene: pop X"
 * the top is whatever scene_top() says */
static void watch_scene(const char *msg)
{
    if (strncmp(msg, "scene: ", 7) != 0) return;
    msg += 7;
    if (strncmp(msg, "set ", 4) == 0) scene_name(msg + 4);
    else if (strncmp(msg, "push ", 5) == 0) scene_name(msg + 5);
    else if (strncmp(msg, "pop ", 4) == 0) {
        const DoomoScene *t = scene_top();
        scene_name(t && t->name ? t->name : "");
    }
}

void dc_log(const char *msg)
{
    if (!msg) return;
    watch_scene(msg);
    ring_put(msg);
    dbgio_write_str(msg);
    dbgio_write_str("\n");
}

void dc_logf(const char *fmt, ...)
{
    char buf[200];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    dc_log(buf);
}

/* ---- platform.h misc ---------------------------------------------------------------- */
void plat_log(const char *msg) { dc_log(msg); }
const char *plat_name(void) { return "DC"; }
uint32_t plat_ticks(void) { return dc_frames; }

uint32_t plat_entropy(void)
{
    static uint32_t mix = 0x9E3779B9u;
    if (dc_entropy_pinned) return dc_entropy_pin;   /* harness "seed N" */
    mix ^= (uint32_t)timer_ns_gettime64() + (dc_vblanks << 16) +
           dc_frames * 2654435761u;
    mix = (mix << 13) | (mix >> 19);
    mix *= 0x85EBCA6Bu;
    mix ^= mix >> 16;
    return mix;
}
