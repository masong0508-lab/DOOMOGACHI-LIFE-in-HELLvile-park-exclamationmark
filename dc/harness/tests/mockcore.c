/* mockcore.c - a tiny libretro core that stands in for Flycast in the
 * harness's own tests (owner: dreamcast).
 *
 * It does what dcrun needs a Dreamcast game to do, without the emulator:
 * asks for a GL core context and clears/scissors the frontend's FBO each
 * frame (so the EGL + FBO + readback path runs), keeps a 16 MB "main RAM"
 * with a DcMailbox at 0x8C010000 (magic, 30 Hz ticks, log ring, scene,
 * commands), reads 4 ports (pad, analog, trigger, mouse, keyboard), rumbles
 * while A is held, and plays a square wave while START is held. It holds
 * (no ticks, commands still run) while ticks == hold_at, like the game.
 * Everything it draws depends only on the tick and the input, so two runs
 * give the same PNGs.
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <GL/glcorearb.h>
#include "../libretro.h"
#include "../dc_mailbox.h"

#define RAM_SIZE (16u << 20)
#define MB_OFF   0x10000u

static retro_environment_t env;
static retro_video_refresh_t video;
static retro_audio_sample_batch_t audio;
static retro_input_poll_t poll_cb;
static retro_input_state_t input;
static struct retro_hw_render_callback hw;
static struct retro_rumble_interface rumble;
static uint8_t *ram;
static volatile DcMailbox *mb;
static unsigned frame_no;
static unsigned devices[4];
static int seed_pinned;
static uint32_t seed_val;
static int saved[8];
static uint32_t town_state;              /* DC4: town-fill / town-crc */

static PFNGLCLEARCOLORPROC p_glClearColor;
static PFNGLCLEARPROC p_glClear;
static PFNGLENABLEPROC p_glEnable;
static PFNGLDISABLEPROC p_glDisable;
static PFNGLSCISSORPROC p_glScissor;
static PFNGLVIEWPORTPROC p_glViewport;
static PFNGLBINDFRAMEBUFFERPROC p_glBindFramebuffer;

static void mlog(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof buf - 1, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n > (int)sizeof buf - 2) n = (int)sizeof buf - 2;
    buf[n++] = '\n';
    for (int i = 0; i < n; i++) mb->log[mb->log_wr++ % DC_MB_LOG_SIZE] = buf[i];
}

static void set_scene(const char *s)
{
    memset((void *)mb->scene, 0, sizeof mb->scene);
    strncpy((char *)mb->scene, s, sizeof mb->scene - 1);
    mlog("scene: set %s", s);
}

static void RETRO_CALLCONV keyboard(bool down, unsigned keycode, uint32_t ch, uint16_t mod)
{
    (void)mod;
    if (down && ch >= 32 && ch < 127) mlog("key: %c", (char)ch);
    else if (down && keycode == RETROK_RETURN) mlog("key: RETURN");
}

static void RETRO_CALLCONV context_reset(void)
{
#define L(n) p_##n = (void *)hw.get_proc_address(#n)
    L(glClearColor); L(glClear); L(glEnable); L(glDisable); L(glScissor);
    L(glViewport); L(glBindFramebuffer);
}

RETRO_API void retro_set_environment(retro_environment_t cb) { env = cb; }
RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb) { video = cb; }
RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio = cb; }
RETRO_API void retro_set_input_poll(retro_input_poll_t cb) { poll_cb = cb; }
RETRO_API void retro_set_input_state(retro_input_state_t cb) { input = cb; }
RETRO_API unsigned retro_api_version(void) { return RETRO_API_VERSION; }

RETRO_API void retro_init(void)
{
    struct retro_keyboard_callback kb = { keyboard };
    ram = calloc(RAM_SIZE, 1);
    mb = (volatile DcMailbox *)(ram + MB_OFF);
    env(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &kb);
}
RETRO_API void retro_deinit(void) { free(ram); ram = NULL; }

RETRO_API void retro_get_system_info(struct retro_system_info *i)
{
    memset(i, 0, sizeof *i);
    i->library_name = "mockcore";
    i->library_version = "1";
    i->valid_extensions = "elf|bin";
}

RETRO_API void retro_get_system_av_info(struct retro_system_av_info *av)
{
    memset(av, 0, sizeof *av);
    av->geometry.base_width = av->geometry.max_width = 640;
    av->geometry.base_height = av->geometry.max_height = 480;
    av->timing.fps = 60;
    av->timing.sample_rate = 44100;
}

RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device)
{
    if (port < 4) devices[port] = device;
}
RETRO_API void retro_reset(void) {}

RETRO_API bool retro_load_game(const struct retro_game_info *g)
{
    struct retro_variable v = { "reicast_cable_type", NULL };
    (void)g;
    memset(&hw, 0, sizeof hw);
    hw.context_type = RETRO_HW_CONTEXT_OPENGL_CORE;
    hw.version_major = 3;
    hw.version_minor = 3;
    hw.context_reset = context_reset;
    hw.depth = true;
    hw.stencil = true;
    hw.bottom_left_origin = true;
    if (!env(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw)) return false;
    if (!env(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &rumble)) rumble.set_rumble_state = NULL;
    /* the frontend must pin the options the harness relies on */
    if (!env(RETRO_ENVIRONMENT_GET_VARIABLE, &v) || !v.value || strcmp(v.value, "VGA (RGB)"))
        return false;
    return true;
}
RETRO_API bool retro_load_game_special(unsigned t, const struct retro_game_info *g, size_t n)
{
    (void)t; (void)g; (void)n;
    return false;
}
RETRO_API void retro_unload_game(void) {}
RETRO_API unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
RETRO_API size_t retro_serialize_size(void) { return 0; }
RETRO_API bool retro_serialize(void *d, size_t s) { (void)d; (void)s; return false; }
RETRO_API bool retro_unserialize(const void *d, size_t s) { (void)d; (void)s; return false; }
RETRO_API void retro_cheat_reset(void) {}
RETRO_API void retro_cheat_set(unsigned i, bool e, const char *c) { (void)i; (void)e; (void)c; }
RETRO_API void *retro_get_memory_data(unsigned id) { return id == RETRO_MEMORY_SYSTEM_RAM ? ram : NULL; }
RETRO_API size_t retro_get_memory_size(unsigned id) { return id == RETRO_MEMORY_SYSTEM_RAM ? RAM_SIZE : 0; }

static void run_cmd(void)
{
    uint32_t cmd = mb->cmd, arg = mb->arg, r = 0;
    char name[20];
    for (int i = 0; i < 20; i++) name[i] = mb->name[i];
    name[19] = 0;
    switch (cmd) {
    case DC_HC_SAVE: if (arg < 8) saved[arg] = 1; r = arg < 8; mlog("save %u", arg); break;
    case DC_HC_LOAD: r = arg < 8 && saved[arg]; break;
    case DC_HC_EXISTS: r = arg < 8 && saved[arg]; break;
    case DC_HC_VERIFY: r = arg < 8 && saved[arg]; break;
    case DC_HC_SCENE: case DC_HC_PUSH:
        if (strcmp(name, "NOPE")) { set_scene(name); r = 1; }
        break;
    case DC_HC_SEED: seed_pinned = 1; seed_val = arg; mlog("seed %u", arg); break;
    case DC_HC_FLAG: r = arg == 7; break;
    case DC_HC_TEXT_FEED: r = arg >= ' ' || arg == '\n'; if (r) mlog("feed %u", arg); break;
    case DC_HC_TOWN_FILL: town_state = arg * 2654435761u + 1u; r = 1; break;
    case DC_HC_TOWN_CRC: r = town_state ^ 0x5A5A5A5Au; mlog("town crc %x", r); break;
    default: r = 0xFFFFFFFFu; break;
    }
    mb->result = r;
    mb->cmd = 0;
}

RETRO_API void retro_run(void)
{
    static int16_t snd[735 * 2];
    static unsigned last_buttons[4];
    unsigned tick;
    float shade;

    poll_cb();
    frame_no++;
    if (frame_no == 10) {
        mb->hold_at = 0xFFFFFFFFu;
        mb->magic = DC_MAILBOX_MAGIC;
        mlog("mock: boot (rtc %ld)", (long)time(NULL));
        set_scene("BOOT");
    }
    if (mb->magic == DC_MAILBOX_MAGIC && mb->hold_at == mb->ticks) {
        if (mb->cmd) run_cmd();         /* "sync on": held at the top of the loop */
    } else if (mb->magic == DC_MAILBOX_MAGIC && frame_no > 10 && (frame_no & 1) == 0) {
        /* (the first tick comes a frame after the magic, as in the game,
         * where doomo_init runs in between) */
        mb->ticks++;
        if (mb->ticks == 15) set_scene("TITLE");
        if (mb->cmd) run_cmd();
    }
    tick = mb->ticks;
    {
        static uint32_t flags_seen = 0xFFFFFFFFu;
        if (mb->magic == DC_MAILBOX_MAGIC && mb->test_flags != flags_seen) {
            flags_seen = mb->test_flags;
            mlog("test flags %u", flags_seen);
        }
    }

    for (unsigned p = 0; p < 4; p++) {
        unsigned b = 0;
        if (devices[p] == RETRO_DEVICE_JOYPAD) {
            b = (unsigned)input(p, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
            if (b != last_buttons[p]) mlog("pad %u: buttons %04x", p + 1, b);
            int16_t lx = input(p, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
            int16_t rt = input(p, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_BUTTON, RETRO_DEVICE_ID_JOYPAD_R2);
            static int16_t last_lx[4], last_rt[4];
            if (lx != last_lx[p]) mlog("pad %u: stick x %d", p + 1, lx);
            if (rt != last_rt[p]) mlog("pad %u: trigger r %d", p + 1, rt);
            last_lx[p] = lx; last_rt[p] = rt;
            if (rumble.set_rumble_state)
                rumble.set_rumble_state(p, RETRO_RUMBLE_STRONG,
                                        (b >> RETRO_DEVICE_ID_JOYPAD_B) & 1 ? 0xC000 : 0);
        } else if (devices[p] == RETRO_DEVICE_MOUSE) {
            int16_t dx = input(p, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
            int16_t dy = input(p, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
            int16_t l = input(p, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
            if (dx || dy || l) mlog("mouse %u: %d %d %s", p + 1, dx, dy, l ? "L" : "-");
        }
        last_buttons[p] = b;
    }

    for (unsigned i = 0; i < 735; i++) {
        int16_t s = (last_buttons[0] >> RETRO_DEVICE_ID_JOYPAD_START) & 1
                    ? (((frame_no * 735 + i) / 50) & 1 ? 8000 : -8000) : 0;
        snd[2 * i] = snd[2 * i + 1] = s;
    }
    audio(snd, 735);

    /* the picture: background shade from the tick, a box that moves with
     * the tick, a white box while any button of port 1 is down */
    p_glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)hw.get_current_framebuffer());
    p_glViewport(0, 0, 640, 480);
    p_glDisable(GL_SCISSOR_TEST);
    shade = (float)(tick % 30) / 30.0f;
    p_glClearColor(0.1f, shade, 0.4f, 1.0f);
    p_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    p_glEnable(GL_SCISSOR_TEST);
    p_glScissor((GLint)(tick * 8 % 560), 200, 80, 80);
    p_glClearColor(1.0f, 0.5f, 0.0f, 1.0f);
    p_glClear(GL_COLOR_BUFFER_BIT);
    if (last_buttons[0]) {
        p_glScissor(0, 0, 64, 64);              /* bottom-left in GL terms */
        p_glClearColor(1, 1, 1, 1);
        p_glClear(GL_COLOR_BUFFER_BIT);
    }
    /* VMU overlay stand-in: a 48x32 checker at the top-left when asked */
    {
        struct retro_variable v = { "reicast_vmu1_screen_display", NULL };
        bool upd = false;
        static int vmu_on;
        if (env(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &upd) && upd &&
            env(RETRO_ENVIRONMENT_GET_VARIABLE, &v) && v.value)
            vmu_on = !strcmp(v.value, "enabled");
        if (vmu_on) {
            for (int y = 0; y < 32; y += 8)
                for (int x = 0; x < 48; x += 8) {
                    float c = ((x + y) / 8) & 1 ? 0.0f : 0.8f;
                    p_glScissor(x, 480 - 32 + y, 8, 8);
                    p_glClearColor(c, c, c, 1);
                    p_glClear(GL_COLOR_BUFFER_BIT);
                }
        }
    }
    p_glDisable(GL_SCISSOR_TEST);
    video(RETRO_HW_FRAME_BUFFER_VALID, 640, 480, 0);
}
