/* dcrun.c - headless libretro frontend for the Dreamcast harness (owner:
 * dreamcast).
 *
 *   dc/harness/build/dcrun CORE.so CONTENT [--script FILE] [--shots DIR]
 *        [--run-dir DIR] [--fresh] [--frames N] [--timeout SEC]
 *        [--mailbox ADDR | --nm FILE] [--wav FILE] [--rtc EPOCH|now]
 *        [--opt KEY=VALUE]... [--device PORT TYPE]... [--half] [--sync]
 *        [--quiet]
 *
 * Boots CONTENT (an .elf, .cdi, .gdi or .chd) in the Flycast libretro core
 * (dc/harness/build_flycast.sh) with no window and no Sega BIOS (Flycast's
 * HLE BIOS), plays a runner script on 4 controller ports, writes PNG
 * screenshots, measures audio, logs rumble and keeps the VMU images in the
 * run directory. Hardware rendering runs on an EGL surfaceless context
 * (Mesa llvmpipe works) into an FBO that is read back after every frame.
 *
 * The game talks to the harness through `dc_mailbox` (dc_mailbox.h) in main
 * RAM: tick counter, plat_log ring, last scene and the command slot. Its
 * address comes from --mailbox, from --nm (sh-elf-nm output) or, for an
 * .elf, from running sh-elf-nm on it. Without a mailbox (a KOS example, say)
 * a tick is two video frames and the log is what the core logs.
 *
 * Script (same syntax as core/tools runner and gba/harness; '#' comments).
 * Times are LOGIC TICKS (30 Hz doomo_frame calls) read from the mailbox:
 *   wait N | frames N           N ticks | N raw 60 Hz video frames
 *   press BTN.. | hold BTN.. N | down BTN.. | release [BTN..]
 *                               press: down 2 ticks, up 1 (sync: down 1,
 *                               up 1, exactly the native runner's)
 *   shot NAME                   SHOTS/NAME.png (640x480, or 320x240 --half)
 *   log TEXT | expect-log TEXT | expect-scene NAME
 *   tips on | off               CL4: the first-time tip cards show in this
 *                               run / stay off (the default; test_flags)
 *   mark-ticks | expect-ticks LO HI   ticks since mark-ticks in [LO, HI]
 *   gaps-reset | expect-gap MS [LABEL]   (Refresh) the presented frames since
 *                    gaps-reset by video frames taken (2 = 30 fps); fail when
 *                    one took over MS ms (a freeze: the game loop blocked)
 *   save N | load N | expect-save N | verify N   (mailbox commands)
 *   save-must-fail N            the save must be refused (result 0; DC4)
 *   shot-during FRAMES NAME     the next mailbox command takes shot NAME
 *                               FRAMES video frames in, while it runs (DC4)
 *   town-fill SEED | town-crc | expect-town-crc   the GBA's save-test
 *                               commands (mailbox 4 and 5; DC4)
 *   scene NAME [ARG] | push NAME [ARG] | seed N | expect-flag F [V]
 *   audio-reset | audio [LABEL] | expect-audio MIN [LABEL]
 *   expect-underruns MAX        DC5: the game's sound underruns plus late
 *                               AICA refills since audio-reset (mailbox
 *                               snd_underruns + snd_starved) <= MAX
 *   expect-sfx MIN              DC5: sound effects started since audio-reset
 *                               (mailbox snd_started) >= MIN
 *   audio-stats [LABEL]         DC5: print the game's sound counters
 * Dreamcast extensions:
 *   port N                      later button/stick/trigger commands use
 *                               controller port N (1-4; default 1)
 *   device N pad|keyboard|mouse|none   what is plugged into port N
 *   stick X Y                   left analog stick, -128..127 (0 0 = centre)
 *   trigger L|R V               analog trigger, 0..255
 *   type "TEXT"                 the native runner's `type`: each char
 *                               goes to the open text entry
 *                               (ui_text_entry_feed, mailbox command),
 *                               then 1 tick; \n = OK, \b = Backspace
 *   key "TEXT"                  type on the keyboard (a port set to
 *                               keyboard), one key every 2 ticks;
 *                               \n = Return, \b = Backspace
 *   sync on [TICK] | off        tick-exact waits, as gba/harness: the game
 *                               holds at the top of its loop (before the
 *                               pad is read) at the end of every wait, so
 *                               keys and commands land on the same tick as
 *                               in the native runner; the first hold is at
 *                               absolute tick TICK (default: the next).
 *                               --sync = "sync on 0" before the script,
 *                               the native runner's start (DC3)
 * --native-seed N: --sync, and at the tick-0 hold the game gets the start
 * `runner --seed N` gets (0x5EED1234 is the runner's default): plat_entropy
 * pinned to N, doomo_rng and the start town as doomo_init() makes them
 *   mouse DX DY [L|R|M|LR..]    move the mouse (a port set to mouse) and
 *                               hold the buttons given for one tick
 *   expect-rumble [PORT]        fail unless rumble was switched on (on
 *                               PORT) since the start / last expect-rumble
 *   vmu-shot NAME               SHOTS/NAME.png: VMU A1's 48x32 LCD
 * Buttons: A B X Y START UP DOWN LEFT RIGHT L R (L/R = full triggers),
 * SELECT (= START+Y, the port's convention: DECISIONS.md).
 *
 * Exit status: 0 ok, 1 failed expectation / hang / timeout, 2 usage or
 * load error.
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>

#include "libretro.h"
#include "dc_mailbox.h"
#include "png.h"

/* ---- options ------------------------------------------------------------ */

static const char *pinned_opts[][2] = {
    /* Flycast's libretro options keep the "reicast_" prefix. */
    { "reicast_system", "dreamcast" },
    { "reicast_use_real_bios", "disabled" },     /* HLE BIOS: no Sega files */
    { "reicast_boot_to_bios", "disabled" },
    { "reicast_cable_type", "VGA (RGB)" },       /* 640x480 progressive     */
    { "reicast_broadcast", "NTSC" },
    { "reicast_region", "USA" },
    { "reicast_language", "English" },
    { "reicast_internal_resolution", "1x" },
    { "reicast_threaded_rendering", "disabled" }, /* frames repeat          */
    { "reicast_frame_skipping", "disabled" },
    { "reicast_texupscale", "disabled" },
    { "reicast_gdrom_fast_loading", "disabled" },
    { "reicast_enable_purupuru", "enabled" },    /* rumble pack in slot 2   */
    { "reicast_per_content_vmus", "disabled" },  /* system/dc/vmu_save_*.bin */
    { "reicast_vmu_sound", "disabled" },
    { "reicast_vmu1_screen_display", "disabled" },
    { "reicast_vmu1_screen_position", "Upper Left" },
    { "reicast_vmu1_screen_size_mult", "1x" },
    { "reicast_vmu1_screen_opacity", "100%" },
    { "reicast_vmu1_pixel_on_color", "BLACK 02" },
    { "reicast_vmu1_pixel_off_color", "DEFAULT_OFF 01" },
    { "reicast_rendering_method", "rasterizer" },
};
#define MAX_OPTS 96
static char opt_key[MAX_OPTS][64], opt_val[MAX_OPTS][64];
static int n_opts, opts_dirty;

static void opt_set(const char *k, const char *v)
{
    int i;
    for (i = 0; i < n_opts; i++)
        if (!strcmp(opt_key[i], k)) break;
    if (i == n_opts) {
        if (n_opts == MAX_OPTS) { fprintf(stderr, "too many options\n"); exit(2); }
        n_opts++;
        snprintf(opt_key[i], sizeof opt_key[i], "%s", k);
    }
    snprintf(opt_val[i], sizeof opt_val[i], "%s", v);
    opts_dirty = 1;
}

static const char *opt_get(const char *k)
{
    for (int i = 0; i < n_opts; i++)
        if (!strcmp(opt_key[i], k)) return opt_val[i];
    return NULL;
}

/* ---- the core ------------------------------------------------------------ */

static struct {
    void *h;
    void (*set_environment)(retro_environment_t);
    void (*set_video_refresh)(retro_video_refresh_t);
    void (*set_audio_sample)(retro_audio_sample_t);
    void (*set_audio_sample_batch)(retro_audio_sample_batch_t);
    void (*set_input_poll)(retro_input_poll_t);
    void (*set_input_state)(retro_input_state_t);
    void (*init)(void);
    void (*deinit)(void);
    void (*get_system_av_info)(struct retro_system_av_info *);
    void (*set_controller_port_device)(unsigned, unsigned);
    bool (*load_game)(const struct retro_game_info *);
    void (*unload_game)(void);
    void (*run)(void);
    void *(*get_memory_data)(unsigned);
    size_t (*get_memory_size)(unsigned);
} core;

static int quiet, fails;
static int sync_mode;           /* "sync on": the game holds at sync_tick */
static uint32_t sync_tick;
#define HOLD_NONE 0xFFFFFFFFu
#define TICK_BUDGET_US 33333u   /* 30 fps (DECISIONS.md FPS TARGET) */
static char run_dir[512], sys_dir[600], save_dir[600], shots_dir[512] = ".";
static int half_shots;
static double timeout_s = 600;
static struct timespec t_start;

static void fail(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputs("FAIL: ", stdout);
    vprintf(fmt, ap);
    fputc('\n', stdout);
    va_end(ap);
    fflush(stdout);
    fails++;
}

static void die(int code, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputs("dcrun: ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(code);
}

/* ---- fixed real-time clock -------------------------------------------------
 * Flycast seeds the Dreamcast RTC from the host's time(). The executable is
 * linked with -rdynamic, so this definition is the one the core binds to:
 * runs repeat, whatever the date. --rtc now restores the host clock. */
static long long rtc_fixed = 1767225600LL;   /* 2026-01-01 00:00:00 UTC */
static int rtc_now;

time_t time(time_t *t)
{
    time_t v;
    if (rtc_now) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        v = ts.tv_sec;
    } else {
        v = (time_t)rtc_fixed;
    }
    if (t) *t = v;
    return v;
}

/* ---- video: EGL surfaceless + FBO ----------------------------------------- */

static struct retro_hw_render_callback hw;
static int hw_on;
static enum retro_pixel_format pix_fmt = RETRO_PIXEL_FORMAT_0RGB1555;
static EGLDisplay egl_dpy = EGL_NO_DISPLAY;
static EGLContext egl_ctx = EGL_NO_CONTEXT;
static GLuint fbo, fbo_tex, fbo_rb;
static unsigned fb_max_w = 640, fb_max_h = 480;
static PFNGLGENFRAMEBUFFERSPROC p_glGenFramebuffers;
static PFNGLBINDFRAMEBUFFERPROC p_glBindFramebuffer;
static PFNGLFRAMEBUFFERTEXTURE2DPROC p_glFramebufferTexture2D;
static PFNGLGENRENDERBUFFERSPROC p_glGenRenderbuffers;
static PFNGLBINDRENDERBUFFERPROC p_glBindRenderbuffer;
static PFNGLRENDERBUFFERSTORAGEPROC p_glRenderbufferStorage;
static PFNGLFRAMEBUFFERRENDERBUFFERPROC p_glFramebufferRenderbuffer;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC p_glCheckFramebufferStatus;
static PFNGLGENTEXTURESPROC p_glGenTextures;
static PFNGLBINDTEXTUREPROC p_glBindTexture;
static PFNGLTEXIMAGE2DPROC p_glTexImage2D;
static PFNGLREADPIXELSPROC p_glReadPixels;
static PFNGLPIXELSTOREIPROC p_glPixelStorei;
static PFNGLGETSTRINGPROC p_glGetString;

static uint8_t *frame;            /* last frame, RGB888                 */
static unsigned frame_w, frame_h;
static uint8_t *readback;
static unsigned long video_frames;

static uintptr_t RETRO_CALLCONV hw_get_fb(void) { return fbo; }
static retro_proc_address_t RETRO_CALLCONV hw_get_proc(const char *sym)
{
    return (retro_proc_address_t)eglGetProcAddress(sym);
}

#define LOADGL(name) \
    if (!(p_##name = (void *)eglGetProcAddress(#name))) die(2, "GL: no " #name)

static void gl_setup(void)
{
    PFNEGLGETPLATFORMDISPLAYEXTPROC get_dpy =
        (void *)eglGetProcAddress("eglGetPlatformDisplayEXT");
    EGLint major, minor, n;
    EGLConfig cfg;
    static const EGLint cfg_attr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                                       EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
                                       EGL_NONE };
    int gles = hw.context_type == RETRO_HW_CONTEXT_OPENGLES2 ||
               hw.context_type == RETRO_HW_CONTEXT_OPENGLES3 ||
               hw.context_type == RETRO_HW_CONTEXT_OPENGLES_VERSION;
    EGLint ctx_attr[16], k = 0;

    if (!get_dpy) die(2, "EGL: no eglGetPlatformDisplayEXT");
    egl_dpy = get_dpy(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
    if (egl_dpy == EGL_NO_DISPLAY || !eglInitialize(egl_dpy, &major, &minor))
        die(2, "EGL: no surfaceless display (Mesa EGL_MESA_platform_surfaceless)");
    if (!eglBindAPI(gles ? EGL_OPENGL_ES_API : EGL_OPENGL_API))
        die(2, "EGL: cannot bind the GL API");
    if (!eglChooseConfig(egl_dpy, cfg_attr, &cfg, 1, &n) || n < 1) cfg = NULL;
    if (hw.context_type == RETRO_HW_CONTEXT_OPENGL_CORE) {
        ctx_attr[k++] = EGL_CONTEXT_MAJOR_VERSION;
        ctx_attr[k++] = hw.version_major ? (EGLint)hw.version_major : 3;
        ctx_attr[k++] = EGL_CONTEXT_MINOR_VERSION;
        ctx_attr[k++] = hw.version_major ? (EGLint)hw.version_minor : 3;
        ctx_attr[k++] = EGL_CONTEXT_OPENGL_PROFILE_MASK;
        ctx_attr[k++] = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT;
    } else if (gles) {
        ctx_attr[k++] = EGL_CONTEXT_MAJOR_VERSION;
        ctx_attr[k++] = hw.context_type == RETRO_HW_CONTEXT_OPENGLES2 ? 2 : 3;
    }
    ctx_attr[k] = EGL_NONE;
    egl_ctx = eglCreateContext(egl_dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
    if (egl_ctx == EGL_NO_CONTEXT) die(2, "EGL: cannot create the GL context");
    if (!eglMakeCurrent(egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, egl_ctx))
        die(2, "EGL: cannot make the context current");

    LOADGL(glGenFramebuffers); LOADGL(glBindFramebuffer);
    LOADGL(glFramebufferTexture2D); LOADGL(glGenRenderbuffers);
    LOADGL(glBindRenderbuffer); LOADGL(glRenderbufferStorage);
    LOADGL(glFramebufferRenderbuffer); LOADGL(glCheckFramebufferStatus);
    LOADGL(glGenTextures); LOADGL(glBindTexture); LOADGL(glTexImage2D);
    LOADGL(glReadPixels); LOADGL(glPixelStorei); LOADGL(glGetString);
    if (!quiet)
        printf("dcrun: EGL %d.%d, GL %s (%s)\n", major, minor,
               (const char *)p_glGetString(GL_VERSION),
               (const char *)p_glGetString(GL_RENDERER));

    p_glGenTextures(1, &fbo_tex);
    p_glBindTexture(GL_TEXTURE_2D, fbo_tex);
    p_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)fb_max_w, (GLsizei)fb_max_h,
                   0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    p_glBindTexture(GL_TEXTURE_2D, 0);
    p_glGenFramebuffers(1, &fbo);
    p_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                             fbo_tex, 0);
    if (hw.depth) {
        p_glGenRenderbuffers(1, &fbo_rb);
        p_glBindRenderbuffer(GL_RENDERBUFFER, fbo_rb);
        p_glRenderbufferStorage(GL_RENDERBUFFER,
                                hw.stencil ? GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT24,
                                (GLsizei)fb_max_w, (GLsizei)fb_max_h);
        p_glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                    hw.stencil ? GL_DEPTH_STENCIL_ATTACHMENT
                                               : GL_DEPTH_ATTACHMENT,
                                    GL_RENDERBUFFER, fbo_rb);
        p_glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }
    if (p_glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        die(2, "GL: the frame buffer object is incomplete");
    p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    readback = malloc((size_t)fb_max_w * fb_max_h * 4);
    if (!readback) die(2, "out of memory");
}

static void frame_size(unsigned w, unsigned h)
{
    if (w == frame_w && h == frame_h && frame) return;
    free(frame);
    frame = calloc((size_t)w * h, 3);
    if (!frame) die(2, "out of memory");
    frame_w = w;
    frame_h = h;
}

static void RETRO_CALLCONV video_refresh(const void *data, unsigned w, unsigned h,
                                         size_t pitch)
{
    video_frames++;
    if (!data || !w || !h) return;               /* dupe: keep the last frame */
    if (data == RETRO_HW_FRAME_BUFFER_VALID) {
        if (w > fb_max_w || h > fb_max_h) die(1, "frame %ux%u > FBO", w, h);
        frame_size(w, h);
        p_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        p_glPixelStorei(GL_PACK_ALIGNMENT, 1);
        p_glReadPixels(0, 0, (GLsizei)w, (GLsizei)h, GL_RGBA, GL_UNSIGNED_BYTE, readback);
        p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
        for (unsigned y = 0; y < h; y++) {
            const uint8_t *s = readback + (size_t)(hw.bottom_left_origin ? h - 1 - y : y) * w * 4;
            uint8_t *d = frame + (size_t)y * w * 3;
            for (unsigned x = 0; x < w; x++, s += 4, d += 3) {
                d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
            }
        }
        return;
    }
    frame_size(w, h);
    for (unsigned y = 0; y < h; y++) {
        const uint8_t *row = (const uint8_t *)data + y * pitch;
        uint8_t *d = frame + (size_t)y * w * 3;
        for (unsigned x = 0; x < w; x++, d += 3) {
            if (pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888) {
                uint32_t p = ((const uint32_t *)row)[x];
                d[0] = (uint8_t)(p >> 16); d[1] = (uint8_t)(p >> 8); d[2] = (uint8_t)p;
            } else {
                uint16_t p = ((const uint16_t *)row)[x];
                unsigned r, g, b;
                if (pix_fmt == RETRO_PIXEL_FORMAT_RGB565) {
                    r = p >> 11; g = (p >> 5) & 63; b = p & 31;
                    d[1] = (uint8_t)((g << 2) | (g >> 4));
                } else {
                    r = (p >> 10) & 31; g = (p >> 5) & 31; b = p & 31;
                    d[1] = (uint8_t)((g << 3) | (g >> 2));
                }
                d[0] = (uint8_t)((r << 3) | (r >> 2));
                d[2] = (uint8_t)((b << 3) | (b >> 2));
            }
        }
    }
}

static void write_shot(const char *name, const uint8_t *rgb, int w, int h)
{
    char path[1200];
    snprintf(path, sizeof path, "%s/%s.png", shots_dir, name);
    if (!png_write_rgb(path, rgb, w, h)) { fail("cannot write %s", path); return; }
    if (!quiet) printf("shot: %s (%dx%d)\n", path, w, h);
}

static void shot(const char *name)
{
    if (!frame) { fail("shot %s: no frame yet", name); return; }
    if (half_shots && frame_w >= 2 && frame_h >= 2) {
        /* 2x2 box filter: the 320x240 logical picture the native runner
         * writes, drawn at 2x by the port */
        int w = (int)frame_w / 2, h = (int)frame_h / 2;
        uint8_t *o = malloc((size_t)w * h * 3);
        if (!o) die(2, "out of memory");
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                for (int c = 0; c < 3; c++) {
                    const uint8_t *s = frame + ((size_t)(2 * y) * frame_w + 2 * x) * 3 + c;
                    o[((size_t)y * w + x) * 3 + c] = (uint8_t)
                        ((s[0] + s[3] + s[frame_w * 3] + s[frame_w * 3 + 3] + 2) / 4);
                }
        write_shot(name, o, w, h);
        free(o);
    } else {
        write_shot(name, frame, (int)frame_w, (int)frame_h);
    }
}

/* ---- input ---------------------------------------------------------------- */

typedef struct {
    unsigned device;          /* RETRO_DEVICE_*                        */
    uint16_t buttons;         /* RETRO_DEVICE_ID_JOYPAD_* bits          */
    int16_t lx, ly;           /* left stick                             */
    int16_t lt, rt;           /* analog triggers, 0..0x7FFF             */
    int16_t mdx, mdy;         /* mouse motion for the next frame        */
    uint8_t mbtn;             /* 1 left, 2 right, 4 middle              */
} Port;
static Port ports[4];
static uint8_t keys[RETROK_LAST];
static struct retro_keyboard_callback kb_cb;
static int cur_port;

static void RETRO_CALLCONV input_poll(void) {}

static int16_t RETRO_CALLCONV input_state(unsigned port, unsigned device,
                                          unsigned index, unsigned id)
{
    if (port >= 4) return 0;
    Port *p = &ports[port];
    switch (device & RETRO_DEVICE_MASK) {
    case RETRO_DEVICE_JOYPAD:
        if (id == RETRO_DEVICE_ID_JOYPAD_MASK) return (int16_t)p->buttons;
        return id < 16 ? (p->buttons >> id) & 1 : 0;
    case RETRO_DEVICE_ANALOG:
        if (index == RETRO_DEVICE_INDEX_ANALOG_LEFT)
            return id == RETRO_DEVICE_ID_ANALOG_X ? p->lx : p->ly;
        if (index == RETRO_DEVICE_INDEX_ANALOG_BUTTON) {
            if (id == RETRO_DEVICE_ID_JOYPAD_L2) return p->lt;
            if (id == RETRO_DEVICE_ID_JOYPAD_R2) return p->rt;
            return id < 16 && ((p->buttons >> id) & 1) ? 0x7FFF : 0;
        }
        return 0;
    case RETRO_DEVICE_MOUSE:
        switch (id) {
        case RETRO_DEVICE_ID_MOUSE_X: return p->mdx;
        case RETRO_DEVICE_ID_MOUSE_Y: return p->mdy;
        case RETRO_DEVICE_ID_MOUSE_LEFT: return p->mbtn & 1;
        case RETRO_DEVICE_ID_MOUSE_RIGHT: return (p->mbtn >> 1) & 1;
        case RETRO_DEVICE_ID_MOUSE_MIDDLE: return (p->mbtn >> 2) & 1;
        default: return 0;
        }
    case RETRO_DEVICE_KEYBOARD:
        return id < RETROK_LAST ? keys[id] : 0;
    default:
        return 0;
    }
}

static void set_device(int port, unsigned dev)
{
    ports[port].device = dev;
    if (core.set_controller_port_device) core.set_controller_port_device((unsigned)port, dev);
}

/* ---- audio ---------------------------------------------------------------- */

static int16_t a_min = 32767, a_max = -32768;
static unsigned long a_frames;
static FILE *wav;
static unsigned long wav_bytes;
static double sample_rate = 44100;

static void audio_take(const int16_t *s, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (s[i] < a_min) a_min = s[i];
        if (s[i] > a_max) a_max = s[i];
    }
    a_frames += n / 2;
    if (wav) wav_bytes += fwrite(s, 2, n, wav) * 2;
}
static size_t RETRO_CALLCONV audio_batch(const int16_t *s, size_t frames)
{
    audio_take(s, frames * 2);
    return frames;
}
static void RETRO_CALLCONV audio_sample(int16_t l, int16_t r)
{
    int16_t s[2] = { l, r };
    audio_take(s, 2);
}
static int audio_p2p(void) { return a_max >= a_min ? a_max - a_min : 0; }
static uint32_t snd_under0, snd_starve0, snd_sfx0;  /* DC5: mailbox snd_underruns / snd_starved / snd_started at audio-reset */

static void wav_header(void)
{
    uint8_t h[44];
    uint32_t rate = (uint32_t)(sample_rate + 0.5), v;
    memcpy(h, "RIFF", 4);
    v = (uint32_t)wav_bytes + 36; memcpy(h + 4, &v, 4);
    memcpy(h + 8, "WAVEfmt ", 8);
    v = 16; memcpy(h + 16, &v, 4);
    h[20] = 1; h[21] = 0; h[22] = 2; h[23] = 0;      /* PCM, stereo */
    memcpy(h + 24, &rate, 4);
    v = rate * 4; memcpy(h + 28, &v, 4);
    h[32] = 4; h[33] = 0; h[34] = 16; h[35] = 0;
    memcpy(h + 36, "data", 4);
    v = (uint32_t)wav_bytes; memcpy(h + 40, &v, 4);
    fseek(wav, 0, SEEK_SET);
    fwrite(h, 1, sizeof h, wav);
    fseek(wav, 0, SEEK_END);
}

/* ---- rumble --------------------------------------------------------------- */

static int rumble_seen[4];
static unsigned long tick_now(void);
static unsigned long tick_mark;    /* mark-ticks */

static bool RETRO_CALLCONV set_rumble(unsigned port, enum retro_rumble_effect e,
                                      uint16_t strength)
{
    static uint16_t last[4][2];
    if (port >= 4 || (unsigned)e > 1) return false;
    if (last[port][e] == strength) return true;
    last[port][e] = strength;
    if (strength) rumble_seen[port] = 1;
    printf("rumble: tick %lu port %u %s %u\n", tick_now(), port + 1,
           e == RETRO_RUMBLE_STRONG ? "strong" : "weak", strength);
    return true;
}

/* ---- logs ----------------------------------------------------------------- */

/* Everything the game logged (mailbox ring, or the core's log without a
 * mailbox), for expect-log. */
static char *glog;
static size_t glog_len, glog_cap;

static void glog_add(const char *s, size_t n)
{
    if (glog_len + n + 1 > glog_cap) {
        glog_cap = (glog_len + n + 1) * 2 + 4096;
        glog = realloc(glog, glog_cap);
        if (!glog) die(2, "out of memory");
    }
    memcpy(glog + glog_len, s, n);
    glog_len += n;
    glog[glog_len] = 0;
}

static volatile DcMailbox *mb;        /* NULL: no mailbox */
static int mb_up;
/* DC4: plat_test_flags() for the game: PLAT_TEST_NO_PICKER (plat_ext.h,
 * bit 0) unless --console, so the first boot of an empty VMU skips the
 * language picker as the native runner does. CL4: and PLAT_TEST_NO_TIPS
 * (bit 1), so the first-time tip cards stay off as in the native runner and
 * the GBA harness, unless the script says "tips on" */
#define TEST_NO_PICKER 1u
#define TEST_NO_TIPS   2u
static uint32_t test_flags = TEST_NO_PICKER | TEST_NO_TIPS;

static void RETRO_CALLCONV core_log(enum retro_log_level lvl, const char *fmt, ...)
{
    char buf[2048];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n >= sizeof buf) n = sizeof buf - 1;
    if (!mb) glog_add(buf, (size_t)n);
    if (!quiet && lvl >= RETRO_LOG_WARN) printf("core: %s", buf);
    else if (quiet < 0) printf("core: %s", buf);
}

/* ---- environment ---------------------------------------------------------- */

static bool RETRO_CALLCONV environment(unsigned cmd, void *data)
{
    switch (cmd & ~(unsigned)RETRO_ENVIRONMENT_EXPERIMENTAL) {
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        struct retro_variable *v = data;
        v->value = opt_get(v->key);
        return v->value != NULL;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        *(bool *)data = opts_dirty != 0;
        opts_dirty = 0;
        return true;
    case RETRO_ENVIRONMENT_SET_VARIABLES:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK:
        return true;                 /* unknown keys fall back to defaults */
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
        *(unsigned *)data = 2;
        return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        pix_fmt = *(const enum retro_pixel_format *)data;
        return true;
    case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER:
        *(unsigned *)data = RETRO_HW_CONTEXT_OPENGL_CORE;
        return true;
    case RETRO_ENVIRONMENT_SET_HW_RENDER: {
        struct retro_hw_render_callback *cb = data;
        if (cb->context_type == RETRO_HW_CONTEXT_VULKAN ||
            cb->context_type == RETRO_HW_CONTEXT_D3D11 ||
            cb->context_type == RETRO_HW_CONTEXT_D3D12 ||
            cb->context_type == RETRO_HW_CONTEXT_NONE)
            return false;
        cb->get_current_framebuffer = hw_get_fb;
        cb->get_proc_address = hw_get_proc;
        hw = *cb;
        hw_on = 1;
        return true;
    }
    case RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT:
        return true;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        *(const char **)data = sys_dir;
        return true;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *(const char **)data = save_dir;
        return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        ((struct retro_log_callback *)data)->log = core_log;
        return true;
    case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE:
        ((struct retro_rumble_interface *)data)->set_rumble_state = set_rumble;
        return true;
    case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK:
        kb_cb = *(const struct retro_keyboard_callback *)data;
        return true;
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
    case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
    case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
    case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
    case RETRO_ENVIRONMENT_SET_ROTATION:
        return true;
    case RETRO_ENVIRONMENT_GET_LANGUAGE:
        *(unsigned *)data = RETRO_LANGUAGE_ENGLISH;
        return true;
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
        return true;
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: {
        const struct retro_system_av_info *av = data;
        sample_rate = av->timing.sample_rate;
        return true;
    }
    case RETRO_ENVIRONMENT_SET_MESSAGE: {
        const struct retro_message *m = data;
        if (!quiet) printf("core message: %s\n", m->msg);
        return true;
    }
    case RETRO_ENVIRONMENT_SET_MESSAGE_EXT: {
        const struct retro_message_ext *m = data;
        if (!quiet) printf("core message: %s\n", m->msg);
        return true;
    }
    case RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION:
        *(unsigned *)data = 1;
        return true;
    default:
        return false;
    }
}

/* ---- mailbox -------------------------------------------------------------- */

static uint32_t mb_addr;
static uint32_t mb_log_rd;
static char mb_line[512];
static size_t mb_line_len;

static void mb_attach(void)
{
    uint8_t *ram;
    size_t size;
    if (!mb_addr || mb) return;
    ram = core.get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
    size = core.get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
    if (!ram || size < 0x10000) die(2, "the core exposes no system RAM");
    if ((mb_addr & 0xFFFFFFu) + sizeof(DcMailbox) > size)
        die(2, "mailbox 0x%08x is outside the %zu bytes of RAM", mb_addr, size);
    mb = (volatile DcMailbox *)(ram + (mb_addr & 0xFFFFFFu));
}

static void mb_poll(void)
{
    uint32_t wr;
    if (!mb) return;
    if (!mb_up) {
        if (mb->magic != DC_MAILBOX_MAGIC) return;
        mb_up = 1;
        mb_log_rd = 0;
        mb->test_flags = test_flags;    /* DC4: a scripted run (plat_test_flags) */
        if (!quiet) printf("dcrun: mailbox up at 0x%08x (video frame %lu)\n", mb_addr, video_frames);
    }
    wr = mb->log_wr;
    if (wr - mb_log_rd > DC_MB_LOG_SIZE) {
        printf("dcrun: log overrun (%u bytes lost)\n", wr - mb_log_rd - DC_MB_LOG_SIZE);
        mb_log_rd = wr - DC_MB_LOG_SIZE;
    }
    while (mb_log_rd != wr) {
        char c = mb->log[mb_log_rd++ % DC_MB_LOG_SIZE];
        if (c == '\n' || mb_line_len == sizeof mb_line - 1) {
            mb_line[mb_line_len++] = '\n';
            glog_add(mb_line, mb_line_len);
            if (quiet <= 0) fwrite(mb_line, 1, mb_line_len, stdout);
            mb_line_len = 0;
        } else {
            mb_line[mb_line_len++] = c;
        }
    }
}

static int find_mailbox_nm(FILE *f)
{
    char line[512];
    while (fgets(line, sizeof line, f)) {
        unsigned long a;
        char t, name[256];
        if (sscanf(line, "%lx %c %255s", &a, &t, name) == 3 && !strcmp(name, "dc_mailbox")) {
            mb_addr = (uint32_t)a;
            return 1;
        }
    }
    return 0;
}

static void locate_mailbox(const char *content, const char *nm_file)
{
    FILE *f;
    if (mb_addr) return;
    if (nm_file) {
        if (!(f = fopen(nm_file, "r"))) die(2, "cannot read %s", nm_file);
        find_mailbox_nm(f);
        fclose(f);
        return;
    }
    size_t n = strlen(content);
    if (n < 4 || strcasecmp(content + n - 4, ".elf")) return;
    {
        char cmd[2400];
        const char *base = getenv("KOS_CC_BASE");
        const char *q;
        for (q = content; *q; q++)
            if (*q == '\'') return;            /* no quoting games */
        if (base && *base)
            snprintf(cmd, sizeof cmd, "'%s/bin/sh-elf-nm' '%s' 2>/dev/null", base, content);
        else
            snprintf(cmd, sizeof cmd, "sh-elf-nm '%s' 2>/dev/null", content);
        if ((f = popen(cmd, "r"))) {
            find_mailbox_nm(f);
            pclose(f);
        }
    }
}

/* ---- running -------------------------------------------------------------- */

static unsigned long frames_run;

static double elapsed(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)(t.tv_sec - t_start.tv_sec) + (t.tv_nsec - t_start.tv_nsec) / 1e9;
}

static unsigned long tick_now(void)
{
    if (mb) return mb_up ? mb->ticks : 0;
    return frames_run / 2;
}

/* DC3: the game's per-tick work (mailbox work_sum / work_n deltas, read
 * after every video frame): ticks over the 30 fps budget, and the worst */
static uint32_t wk_sum0, wk_n0;
static unsigned long wk_over, wk_ticks;
static uint32_t wk_worst, wk_worst_tick;
static char wk_worst_scene[25];

static void work_poll(void)
{
    uint32_t n, sum, w;
    if (!mb || !mb_up) return;
    n = mb->work_n;
    sum = mb->work_sum;
    if (n == wk_n0) return;
    w = (sum - wk_sum0) / (n - wk_n0);  /* mean when several ticks ended in one frame */
    wk_ticks += n - wk_n0;
    if (w > TICK_BUDGET_US) {
        wk_over += n - wk_n0;
        if (!quiet) printf("work: tick %u took %u us (over the 30 fps budget)\n", mb->ticks, w);
    }
    if (w > wk_worst) {
        wk_worst = w;
        wk_worst_tick = mb->ticks;
        for (int i = 0; i < 24; i++) wk_worst_scene[i] = mb->scene[i];
        wk_worst_scene[24] = 0;
    }
    wk_sum0 = sum;
    wk_n0 = n;
}

/* Refresh: the video frames between two presented frames (mailbox drawn), since gaps-reset.
 * Unlike the work counters this sees a stall outside plat_begin_frame..plat_end_frame (a VMU write
 * in dc_vmu_poll). */
#define GAP_MAX 64
static unsigned long gp_h[GAP_MAX + 1], gp_f0, gp_worst, gp_worst_tick;
static uint32_t gp_d0;
static int gp_on;

static void gap_poll(void)
{
    uint32_t d;
    if (!mb || !mb_up) return;
    d = mb->drawn;
    if (d == gp_d0) return;
    if (gp_on) {
        unsigned long g = frames_run - gp_f0;
        gp_h[g > GAP_MAX ? GAP_MAX : g]++;
        if (g > gp_worst) { gp_worst = g; gp_worst_tick = mb->ticks; }
    }
    gp_d0 = d;
    gp_f0 = frames_run;
    gp_on = 1;
}

static void run_frame(void)
{
    core.run();
    frames_run++;
    for (int i = 0; i < 4; i++) ports[i].mdx = ports[i].mdy = 0;
    mb_poll();
    work_poll();
    gap_poll();
    if ((frames_run & 63) == 0 && elapsed() > timeout_s) {
        fail("timeout: %.0f s of wall time (tick %lu)", timeout_s, tick_now());
        exit(1);
    }
}

/* Run until the game's mailbox is up (the commands and sync need it). */
static int mb_wait_up(void)
{
    unsigned long f = 0;
    if (!mb) return 0;
    while (!mb_up) {
        run_frame();
        if (++f > 3600) { fail("the game never wrote the mailbox magic"); return 0; }
    }
    return 1;
}

/* sync mode: the input the game read at the last release of the hold */
static Port sync_ports[4];
static uint8_t sync_keys[RETROK_LAST];   /* DC4: the keyboard too */

/* sync mode: release the hold and run until the game holds again at
 * absolute tick `target`. The pad reaches the game through KOS's maple
 * poll (one per vblank), so when the input changed while the game was
 * held, the hold lasts 3 more video frames first: the game then reads
 * the new input on the tick it was given on, as the native runner does. */
static int run_until_tick(uint32_t target)
{
    unsigned long f = 0;
    uint32_t last = mb->ticks;
    if (memcmp(sync_ports, ports, sizeof ports) || memcmp(sync_keys, keys, sizeof keys)) {
        for (int i = 0; i < 3; i++) run_frame();
        memcpy(sync_ports, ports, sizeof ports);
        memcpy(sync_keys, keys, sizeof keys);
    }
    __sync_synchronize();
    mb->hold_at = target;
    while ((int32_t)(mb->ticks - target) < 0) {
        run_frame();
        if (mb->ticks != last) { last = mb->ticks; f = 0; }
        else if (++f > 1200) {
            fail("hang: no logic tick for 1200 video frames (tick %u, sync)", mb->ticks);
            return 0;
        }
    }
    sync_tick = target;
    return 1;
}

static int sync_on(uint32_t at)
{
    if (!mb_wait_up()) { fail("sync: no game mailbox"); return 0; }
    if (mb->hold_at != HOLD_NONE) { fail("sync: the game already holds"); return 0; }
    if ((int32_t)(at - mb->ticks) < 0 || (at == mb->ticks && at != 0)) {
        fail("sync on %u: the game is already at tick %u", at, mb->ticks);
        return 0;
    }
    mb->draw_all = 1;                   /* a held game must not skip a draw to catch up */
    sync_mode = 1;
    memcpy(sync_ports, ports, sizeof ports);
    memcpy(sync_keys, keys, sizeof keys);
    if (at == mb->ticks) {              /* tick 0: hold before the first tick */
        mb->hold_at = at;
        sync_tick = at;
        return 1;
    }
    return run_until_tick(at);
}

static void sync_off(void)
{
    if (!sync_mode) return;
    mb->hold_at = HOLD_NONE;
    mb->draw_all = 0;
    sync_mode = 0;
}

/* Run until n more logic ticks have passed. Returns 0 on a hang. */
static int run_ticks(unsigned long n)
{
    unsigned long start = tick_now(), limit = n * 10 + 1200, f = 0;
    if (sync_mode) return run_until_tick(sync_tick + (uint32_t)n);
    while (tick_now() - start < n) {
        run_frame();
        if (++f > limit) {
            fail("hang: tick %lu after %lu video frames (wanted %lu more ticks)",
                 tick_now(), f, n - (tick_now() - start));
            return 0;
        }
    }
    return 1;
}

/* DC4: `shot-during FRAMES NAME`: the next mailbox command takes a shot
 * FRAMES video frames after it was posted, while the game is still busy
 * with it (the VMU card of a save or a load) */
static unsigned long during_frames;
static char during_name[128];

static void shot(const char *name);

/* Post a mailbox command and run until the game clears it. */
static int mb_cmd(uint32_t cmd, uint32_t arg, const char *name, uint32_t *result)
{
    unsigned long f = 0;
    if (!mb) { fail("command %u needs the game's mailbox (--nm or --mailbox)", cmd); return 0; }
    if (!mb_wait_up()) return 0;
    for (int i = 0; i < 20; i++) mb->name[i] = 0;
    if (name)
        for (int i = 0; i < 19 && name[i]; i++) mb->name[i] = name[i];
    mb->arg = arg;
    mb->result = 0;
    __sync_synchronize();
    mb->cmd = cmd;
    while (mb->cmd) {
        run_frame();
        ++f;
        if (during_frames && f == during_frames) {
            shot(during_name);
            during_frames = 0;
        }
        if (f > 3600) { fail("mailbox command %u never finished", cmd); return 0; }
    }
    if (during_frames) {
        fail("shot-during %s: the command ended after %lu frames", during_name, f);
        during_frames = 0;
    }
    if (result) *result = mb->result;
    return 1;
}

/* ---- keyboard ------------------------------------------------------------- */

static void key_event(unsigned code, uint32_t ch, int down)
{
    if (code < RETROK_LAST) keys[code] = (uint8_t)down;
    if (kb_cb.callback)
        kb_cb.callback(down != 0, code, down ? ch : 0,
                       ch >= 'A' && ch <= 'Z' ? RETROKMOD_SHIFT : 0);
}

static int type_text(const char *s)
{
    int kb = 0;
    for (int i = 0; i < 4; i++)
        if (ports[i].device == RETRO_DEVICE_KEYBOARD) kb = 1;
    if (!kb) { fail("type: no port is set to keyboard (device N keyboard)"); return 0; }
    for (; *s; s++) {
        unsigned c = (unsigned char)*s, code;
        int shift = 0;
        if (c == '\\' && s[1]) {
            s++;
            c = *s == 'n' ? '\r' : *s == 'b' ? '\b' : (unsigned char)*s;
        }
        if (c >= 'A' && c <= 'Z') { code = c - 'A' + 'a'; shift = 1; }
        else if (c == '\r') code = RETROK_RETURN;
        else if (c == '\b') code = RETROK_BACKSPACE;
        else code = c;
        if (shift) key_event(RETROK_LSHIFT, 0, 1);
        key_event(code, c == '\r' || c == '\b' ? 0 : c, 1);
        if (!run_ticks(1)) return 0;
        key_event(code, 0, 0);
        if (shift) key_event(RETROK_LSHIFT, 0, 0);
        if (!run_ticks(1)) return 0;
    }
    return 1;
}

/* ---- buttons -------------------------------------------------------------- */

static int parse_buttons(char **v, int n, uint16_t *mask, int *lt, int *rt)
{
    static const struct { const char *n; uint16_t m; } tab[] = {
        /* Flycast maps RetroPad B to Dreamcast A, A to B, Y to X, X to Y */
        { "A", 1u << RETRO_DEVICE_ID_JOYPAD_B }, { "B", 1u << RETRO_DEVICE_ID_JOYPAD_A },
        { "X", 1u << RETRO_DEVICE_ID_JOYPAD_Y }, { "Y", 1u << RETRO_DEVICE_ID_JOYPAD_X },
        { "START", 1u << RETRO_DEVICE_ID_JOYPAD_START },
        { "SELECT", (1u << RETRO_DEVICE_ID_JOYPAD_START) | (1u << RETRO_DEVICE_ID_JOYPAD_X) },
        { "UP", 1u << RETRO_DEVICE_ID_JOYPAD_UP }, { "DOWN", 1u << RETRO_DEVICE_ID_JOYPAD_DOWN },
        { "LEFT", 1u << RETRO_DEVICE_ID_JOYPAD_LEFT }, { "RIGHT", 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT },
        { "L", 1u << RETRO_DEVICE_ID_JOYPAD_L2 }, { "R", 1u << RETRO_DEVICE_ID_JOYPAD_R2 },
    };
    *mask = 0;
    *lt = *rt = 0;
    for (int i = 0; i < n; i++) {
        size_t k;
        for (k = 0; k < sizeof tab / sizeof tab[0]; k++)
            if (!strcasecmp(v[i], tab[k].n)) break;
        if (k == sizeof tab / sizeof tab[0]) { fail("unknown button '%s'", v[i]); return 0; }
        *mask |= tab[k].m;
        if (!strcasecmp(v[i], "L")) *lt = 1;
        if (!strcasecmp(v[i], "R")) *rt = 1;
    }
    return 1;
}

static void buttons_down(uint16_t m, int lt, int rt)
{
    Port *p = &ports[cur_port];
    p->buttons |= m;
    if (lt) p->lt = 0x7FFF;
    if (rt) p->rt = 0x7FFF;
}

static void buttons_up(uint16_t m, int lt, int rt)
{
    Port *p = &ports[cur_port];
    p->buttons &= (uint16_t)~m;
    if (lt) p->lt = 0;
    if (rt) p->rt = 0;
}

/* ---- VMU LCD ---------------------------------------------------------------
 * Flycast draws VMU A1's LCD over the picture when the overlay option is on
 * (Upper Left, 1x, opaque: 48x32 at 0,0 of the 640x480 frame). The overlay
 * is switched on for one frame, cropped and switched off again. */
static void vmu_shot(const char *name)
{
    uint8_t lcd[48 * 32 * 3];
    opt_set("reicast_vmu1_screen_display", "enabled");
    run_frame();
    run_frame();
    if (!frame || frame_w < 48 || frame_h < 32) { fail("vmu-shot: no frame"); return; }
    for (int y = 0; y < 32; y++)
        memcpy(lcd + y * 48 * 3, frame + (size_t)y * frame_w * 3, 48 * 3);
    opt_set("reicast_vmu1_screen_display", "disabled");
    run_frame();
    write_shot(name, lcd, 48, 32);
}

/* ---- the script ----------------------------------------------------------- */

static int tokenize(char *s, char **v, int max)
{
    int n = 0;
    while (*s && n < max) {
        while (isspace((unsigned char)*s)) s++;
        if (!*s || *s == '#') break;
        if (*s == '"' || *s == '\'') {
            char q = *s++;
            v[n++] = s;
            while (*s && *s != q) {
                if (*s == '\\' && s[1]) s++;
                s++;
            }
            if (*s) *s++ = 0;
        } else {
            v[n++] = s;
            while (*s && !isspace((unsigned char)*s)) s++;
            if (*s) *s++ = 0;
        }
    }
    return n;
}

/* The rest of the line after the command word, for log / expect-log. */
static const char *rest_of(const char *line, const char *word)
{
    const char *p = strstr(line, word);
    if (!p) return "";
    p += strlen(word);
    while (isspace((unsigned char)*p)) p++;
    return p;
}

static void chomp(char *s)
{
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ')) s[--n] = 0;
}

static int scene_is(const char *name)
{
    char pat[96];
    if (mb && mb_up) {
        char sc[25];
        for (int i = 0; i < 24; i++) sc[i] = mb->scene[i];
        sc[24] = 0;
        if (!strcmp(sc, name)) return 1;
    }
    snprintf(pat, sizeof pat, "scene: set %s", name);
    if (glog && strstr(glog, pat)) return 1;
    snprintf(pat, sizeof pat, "scene: %s", name);
    return glog && strstr(glog, pat);
}

static int run_script(FILE *f)
{
    char line[1024], raw[1024], *v[32];
    int lineno = 0, town_crc_set = 0;
    uint32_t town_crc = 0;
    while (fgets(line, sizeof line, f)) {
        int n;
        lineno++;
        chomp(line);
        snprintf(raw, sizeof raw, "%s", line);
        n = tokenize(line, v, 32);
        if (!n) continue;
        const char *c = v[0];
        uint16_t m;
        int lt, rt;
        uint32_t r;
        if (!strcmp(c, "wait") && n >= 2) {
            if (!run_ticks(strtoul(v[1], NULL, 0))) return 0;
        } else if (!strcmp(c, "frames") && n >= 2) {
            for (unsigned long i = strtoul(v[1], NULL, 0); i; i--) run_frame();
        } else if (!strcmp(c, "gaps-reset")) {
            memset(gp_h, 0, sizeof(gp_h));
            gp_worst = gp_worst_tick = 0;
        } else if (!strcmp(c, "expect-gap") && n >= 2) {
            double most = strtod(v[1], NULL), ms = gp_worst * 1000.0 / 59.94;
            printf("gaps%s%s: video-frames-per-frame histogram", n >= 3 ? " " : "", n >= 3 ? v[2] : "");
            for (int g = 1; g <= GAP_MAX; g++)
                if (gp_h[g]) printf(" %d%s:%lu", g, g == GAP_MAX ? "+" : "", gp_h[g]);
            printf(" max %lu (%.1f ms) at tick %lu\n", gp_worst, ms, gp_worst_tick);
            if (ms > most) fail("line %d: a frame took %.1f ms (expect-gap %s)", lineno, ms, v[1]);
        } else if (!strcmp(c, "mark-ticks")) {
            tick_mark = tick_now();
        } else if (!strcmp(c, "expect-ticks") && n >= 3) {
            unsigned long d = tick_now() - tick_mark;
            unsigned long lo = strtoul(v[1], NULL, 0), hi = strtoul(v[2], NULL, 0);
            printf("ticks: %lu since mark-ticks (want %lu..%lu)\n", d, lo, hi);
            if (d < lo || d > hi) fail("line %d: expect-ticks %lu..%lu got %lu", lineno, lo, hi, d);
        } else if (!strcmp(c, "press") && n >= 2) {
            if (!parse_buttons(v + 1, n - 1, &m, &lt, &rt)) return 0;
            /* sync: the native runner's press, 1 tick down + 1 tick up;
             * unsynced, 2 ticks down so the maple poll cannot miss it */
            buttons_down(m, lt, rt);
            if (!run_ticks(sync_mode ? 1 : 2)) return 0;
            buttons_up(m, lt, rt);
            if (!run_ticks(1)) return 0;
        } else if (!strcmp(c, "hold") && n >= 3) {
            if (!parse_buttons(v + 1, n - 2, &m, &lt, &rt)) return 0;
            buttons_down(m, lt, rt);
            if (!run_ticks(strtoul(v[n - 1], NULL, 0))) return 0;
            buttons_up(m, lt, rt);
        } else if (!strcmp(c, "down") && n >= 2) {
            if (!parse_buttons(v + 1, n - 1, &m, &lt, &rt)) return 0;
            buttons_down(m, lt, rt);
        } else if (!strcmp(c, "release")) {
            if (n == 1) {
                ports[cur_port].buttons = 0;
                ports[cur_port].lt = ports[cur_port].rt = 0;
            } else {
                if (!parse_buttons(v + 1, n - 1, &m, &lt, &rt)) return 0;
                buttons_up(m, lt, rt);
            }
        } else if (!strcmp(c, "shot") && n >= 2) {
            /* sync: the game is held; 2 video frames show the last tick's
             * picture (the PVR renders and flips it after the submit) */
            if (sync_mode) { run_frame(); run_frame(); }
            shot(v[1]);
        } else if (!strcmp(c, "log")) {
            printf("%s\n", rest_of(raw, "log"));
        } else if (!strcmp(c, "expect-log") && n >= 2) {
            const char *t = rest_of(raw, "expect-log");
            if (!glog || !strstr(glog, t)) fail("line %d: expect-log '%s' not in the log", lineno, t);
        } else if (!strcmp(c, "expect-scene") && n >= 2) {
            if (!scene_is(v[1])) fail("line %d: expect-scene %s", lineno, v[1]);
        } else if ((!strcmp(c, "save") || !strcmp(c, "load") || !strcmp(c, "expect-save") ||
                    !strcmp(c, "verify")) && n >= 2) {
            uint32_t id = c[0] == 's' ? DC_HC_SAVE : c[0] == 'l' ? DC_HC_LOAD :
                          c[0] == 'e' ? DC_HC_EXISTS : DC_HC_VERIFY;
            if (!mb_cmd(id, (uint32_t)strtoul(v[1], NULL, 0), NULL, &r)) return 0;
            if (!r)   /* GBA convention: nonzero = success / exists */
                fail("line %d: %s %s gave %u", lineno, c, v[1], r);
        } else if (!strcmp(c, "shot-during") && n >= 3) {
            during_frames = strtoul(v[1], NULL, 0);
            snprintf(during_name, sizeof during_name, "%s", v[2]);
        } else if (!strcmp(c, "save-must-fail") && n >= 2) {
            if (!mb_cmd(DC_HC_SAVE, (uint32_t)strtoul(v[1], NULL, 0), NULL, &r)) return 0;
            if (r) fail("line %d: save %s should have been refused", lineno, v[1]);
        } else if (!strcmp(c, "town-fill") && n >= 2) {
            if (!mb_cmd(DC_HC_TOWN_FILL, (uint32_t)strtoul(v[1], NULL, 0), NULL, NULL)) return 0;
        } else if (!strcmp(c, "town-crc") || !strcmp(c, "expect-town-crc")) {
            if (!mb_cmd(DC_HC_TOWN_CRC, 0, NULL, &r)) return 0;
            if (c[0] == 't') { town_crc = r; town_crc_set = 1; }
            else if (!town_crc_set) fail("line %d: expect-town-crc before any town-crc", lineno);
            else if (r != town_crc)
                fail("line %d: town crc 0x%08x, expected 0x%08x", lineno, r, town_crc);
        } else if ((!strcmp(c, "scene") || !strcmp(c, "push")) && n >= 2) {
            if (!mb_cmd(c[0] == 's' ? DC_HC_SCENE : DC_HC_PUSH,
                        n >= 3 ? (uint32_t)strtoul(v[2], NULL, 0) : 0, v[1], &r)) return 0;
            if (!r) fail("line %d: %s %s: unknown scene", lineno, c, v[1]);
        } else if (!strcmp(c, "seed") && n >= 2) {
            if (!mb_cmd(DC_HC_SEED, (uint32_t)strtoul(v[1], NULL, 0), NULL, NULL)) return 0;
        } else if (!strcmp(c, "expect-flag") && n >= 2) {
            uint32_t want = n >= 3 ? (uint32_t)strtoul(v[2], NULL, 0) : 1;
            if (!mb_cmd(DC_HC_FLAG, (uint32_t)strtoul(v[1], NULL, 0), NULL, &r)) return 0;
            if (r != want) fail("line %d: flag %s = %u, wanted %u", lineno, v[1], r, want);
        } else if (!strcmp(c, "audio-reset")) {
            a_min = 32767; a_max = -32768; a_frames = 0;
            snd_under0 = mb && mb_up ? mb->snd_underruns : 0;
            snd_starve0 = mb && mb_up ? mb->snd_starved : 0;
            snd_sfx0 = mb && mb_up ? mb->snd_started : 0;
        } else if (!strcmp(c, "expect-sfx") && n >= 2) {
            uint32_t got = mb && mb_up ? mb->snd_started - snd_sfx0 : 0;
            printf("audio: %u sound effects started since audio-reset (min %s)\n", got, v[1]);
            if (!mb || !mb_up) fail("line %d: expect-sfx needs the game's mailbox", lineno);
            else if (got < strtoul(v[1], NULL, 0))
                fail("line %d: expect-sfx %s: got %u", lineno, v[1], got);
        } else if (!strcmp(c, "expect-underruns") && n >= 2) {
            uint32_t un = mb && mb_up ? mb->snd_underruns - snd_under0 : 0;
            uint32_t st = mb && mb_up ? mb->snd_starved - snd_starve0 : 0, got = un + st;
            printf("audio: %u sound underruns and %u late AICA refills since audio-reset (max %s together)\n",
                   un, st, v[1]);
            if (!mb || !mb_up) fail("line %d: expect-underruns needs the game's mailbox", lineno);
            else if (got > strtoul(v[1], NULL, 0))
                fail("line %d: expect-underruns %s: got %u", lineno, v[1], got);
        } else if (!strcmp(c, "audio-stats")) {
            if (mb && mb_up)
                printf("audio-stats: %s slots %u/%u ring max %u underruns %u overruns %u dropped %u "
                       "gaps %u frames content %u clips %u effects %u hash 0x%08x hash600 0x%08x "
                       "mixer %u us/tick (%u ticks, max %u) refill gap max %u us, late refills %u\n",
                       n >= 2 ? v[1] : "", mb->snd_consumed, mb->snd_pushed, mb->snd_queue_max,
                       mb->snd_underruns, mb->snd_overruns, mb->snd_dropped, mb->snd_gap_frames,
                       mb->snd_frames, mb->snd_clips, mb->snd_started, mb->snd_hash, mb->snd_hash600,
                       mb->snd_us_n ? mb->snd_us_sum / mb->snd_us_n : 0u, mb->snd_us_n, mb->snd_us_max,
                       mb->snd_cb_gap_max, mb->snd_starved);
        } else if (!strcmp(c, "audio")) {
            printf("audio: %s peak-to-peak %d over %lu frames\n", n >= 2 ? v[1] : "",
                   audio_p2p(), a_frames);
        } else if (!strcmp(c, "expect-audio") && n >= 2) {
            int min = atoi(v[1]);
            printf("audio: %s peak-to-peak %d (min %d)\n", n >= 3 ? v[2] : "", audio_p2p(), min);
            if (audio_p2p() < min) fail("line %d: expect-audio %d: got %d", lineno, min, audio_p2p());
        } else if (!strcmp(c, "port") && n >= 2) {
            int p = atoi(v[1]);
            if (p < 1 || p > 4) { fail("line %d: port 1-4", lineno); return 0; }
            cur_port = p - 1;
        } else if (!strcmp(c, "device") && n >= 3) {
            int p = atoi(v[1]);
            unsigned d = !strcmp(v[2], "keyboard") ? RETRO_DEVICE_KEYBOARD :
                         !strcmp(v[2], "mouse") ? RETRO_DEVICE_MOUSE :
                         !strcmp(v[2], "none") ? RETRO_DEVICE_NONE : RETRO_DEVICE_JOYPAD;
            if (p < 1 || p > 4) { fail("line %d: port 1-4", lineno); return 0; }
            set_device(p - 1, d);
        } else if (!strcmp(c, "stick") && n >= 3) {
            long x = strtol(v[1], NULL, 0), y = strtol(v[2], NULL, 0);
            ports[cur_port].lx = (int16_t)(x * 256 + (x > 0 ? 255 : 0));
            ports[cur_port].ly = (int16_t)(y * 256 + (y > 0 ? 255 : 0));
        } else if (!strcmp(c, "trigger") && n >= 3) {
            long t = strtol(v[2], NULL, 0);
            int16_t val = (int16_t)(t <= 0 ? 0 : t >= 255 ? 0x7FFF : t * 128);
            if (toupper((unsigned char)v[1][0]) == 'L') ports[cur_port].lt = val;
            else ports[cur_port].rt = val;
        } else if (!strcmp(c, "key") && n >= 2) {
            if (!type_text(v[1])) return 0;
        /* --- DC7: the native runner's story commands (side_sample_a.txt);
         * text comparisons use the first 19 characters (mailbox name) --- */
        } else if (!strcmp(c, "side") && n >= 2) {
            uint32_t mm = (uint32_t)strtoul(v[1], NULL, 0), st = n > 2 ? (uint32_t)strtoul(v[2], NULL, 0) : 3;
            if (!mb_cmd(DC_HC_SIDE, mm | st << 16, NULL, &r)) return 0;
            if (!r) fail("line %d: side needs a mission 100..217, got %s", lineno, v[1]);
        } else if (!strcmp(c, "talk") && n >= 2) {
            if (!mb_cmd(DC_HC_TALK, (uint32_t)strtol(v[1], NULL, 0), NULL, &r)) return 0;
        } else if (!strcmp(c, "cell") && n >= 3) {
            uint32_t cx = (uint32_t)strtoul(v[1], NULL, 0), cy = (uint32_t)strtoul(v[2], NULL, 0);
            if (!mb_cmd(DC_HC_CELL, (cx & 0xFFFFu) | cy << 16, NULL, &r)) return 0;
        } else if (!strcmp(c, "expect-text") && n >= 2) {
            if (!mb_cmd(DC_HC_SAYS, 0, v[1], &r)) return 0;
            if (r != 2) fail("line %d: expect-text: the dialogue does not say \"%.19s\"", lineno, v[1]);
        } else if (!strcmp(c, "skip-talk") || !strcmp(c, "skip-to") || !strcmp(c, "skip-to-scene")) {
            /* press A (1 tick, then 10 released, as the native runner) until
             * the conversation is over / the dialogue says TEXT / NAME is on top */
            int talk = c[5] == 't' && c[6] == 'a', scene = !talk && c[7] == '-';
            const char *want = talk ? "" : (n >= 2 ? v[1] : "");
            unsigned long max = strtoul(talk ? (n >= 2 ? v[1] : "120") : (n >= 3 ? v[2] : "80"), NULL, 0), k;
            uint16_t am;
            char *aname[1] = { (char *)"A" };
            if (scene && !strncmp(want, "SCENE_", 6)) want += 6;
            if (!parse_buttons(aname, 1, &am, &lt, &rt)) return 0;
            for (k = 0; ; k++) {
                int done;
                if (talk) {
                    if (!mb_cmd(DC_HC_STORY_STATE, 0, NULL, &r)) return 0;
                    done = r == 3;          /* not talking, the overworld on top */
                } else {
                    if (!mb_cmd(scene ? DC_HC_TOP_IS : DC_HC_SAYS, 0, want, &r)) return 0;
                    done = r == 2;
                }
                if (done) break;
                if (k == max) {
                    fail("line %d: %s %s: %s", lineno, c, want,
                         talk ? "the conversation never ended" : scene ? "never reached the scene" : "the dialogue never said it");
                    break;
                }
                buttons_down(am, 0, 0);
                if (!run_ticks(1)) return 0;
                buttons_up(am, 0, 0);
                if (!run_ticks(10)) return 0;
            }
        } else if (!strcmp(c, "clock") && n >= 2) {
            long hh = strtol(v[1], NULL, 10), mi = n > 2 ? strtol(v[2], NULL, 10) : 0;
            if (!mb_cmd(DC_HC_CLOCK, (uint32_t)(((hh * 60 + mi) % 1440 + 1440) % 1440), NULL, &r)) return 0;
        } else if (!strcmp(c, "no-stubs")) {
            if (glog && strstr(glog, "STUB:")) fail("line %d: no-stubs: a core stub ran (STUB: in the log)", lineno);
        } else if (!strcmp(c, "type") && n >= 2) {
            /* the native runner's type: each char to the open text entry,
             * then one tick */
            for (const char *t = v[1]; *t; t++) {
                unsigned ch = (unsigned char)*t;
                if (ch == '\\' && t[1]) {
                    t++;
                    ch = *t == 'n' ? '\n' : *t == 'b' ? '\b' : (unsigned char)*t;
                }
                if (!mb_cmd(DC_HC_TEXT_FEED, ch, NULL, &r)) return 0;
                if (!r) { fail("line %d: type: no text entry open", lineno); break; }
                if (!run_ticks(1)) return 0;
            }
        } else if (!strcmp(c, "tips") && n >= 2) {
            /* CL4: tips on | off: let the first-time tip cards show in this run (the native
             * runner's and the GBA harness's "tips"); the game reads the word every time */
            if (!strcmp(v[1], "on")) test_flags &= ~TEST_NO_TIPS;
            else if (!strcmp(v[1], "off")) test_flags |= TEST_NO_TIPS;
            else fail("line %d: tips needs on or off, got %s", lineno, v[1]);
            if (mb && mb_up) mb->test_flags = test_flags;
        } else if (!strcmp(c, "sync") && n >= 2) {
            if (!strcmp(v[1], "on") && !sync_mode) {
                if (!mb_wait_up()) return 0;
                if (!sync_on(n >= 3 ? (uint32_t)strtoul(v[2], NULL, 0) : mb->ticks + 1)) return 0;
            } else if (!strcmp(v[1], "off")) {
                sync_off();
            }
        } else if (!strcmp(c, "mouse") && n >= 3) {
            Port *p = &ports[cur_port];
            p->mbtn = 0;
            for (const char *b = n >= 4 ? v[3] : ""; *b; b++)
                p->mbtn |= *b == 'L' ? 1 : *b == 'R' ? 2 : *b == 'M' ? 4 : 0;
            if (p->device != RETRO_DEVICE_MOUSE) fail("line %d: port %d is not a mouse", lineno, cur_port + 1);
            p->mdx = (int16_t)atoi(v[1]);   /* read by the next frame only */
            p->mdy = (int16_t)atoi(v[2]);
            run_frame();
            if (!run_ticks(1)) return 0;
            p->mbtn = 0;
        } else if (!strcmp(c, "expect-rumble")) {
            int p = n >= 2 ? atoi(v[1]) : 0, seen = 0;
            for (int i = 0; i < 4; i++)
                if ((!p || p == i + 1) && rumble_seen[i]) seen = 1;
            if (!seen) fail("line %d: expect-rumble: no rumble%s", lineno, p ? " on that port" : "");
            memset(rumble_seen, 0, sizeof rumble_seen);
        } else if (!strcmp(c, "vmu-shot") && n >= 2) {
            vmu_shot(v[1]);
        } else {
            fail("line %d: unknown command '%s'", lineno, raw);
            return 0;
        }
        fflush(stdout);
    }
    return 1;
}

/* ---- main ----------------------------------------------------------------- */

static void mkdirs(const char *path)
{
    char tmp[1200];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = tmp + 1; *p; p++)
        if (*p == '/') { *p = 0; mkdir(tmp, 0755); *p = '/'; }
    mkdir(tmp, 0755);
}

static void usage(void)
{
    fprintf(stderr,
        "usage: dcrun CORE.so CONTENT [--script FILE] [--shots DIR] [--run-dir DIR]\n"
        "             [--fresh] [--frames N] [--timeout SEC] [--mailbox ADDR | --nm FILE]\n"
        "             [--wav FILE] [--rtc EPOCH|now] [--opt KEY=VALUE]... [--half] [--sync]\n"
        "             [--native-seed N] [--console] [--quiet]\n"
        "             [--device PORT pad|keyboard|mouse|none]... [--verbose]\n");
    exit(2);
}

int main(int argc, char **argv)
{
    const char *core_path, *content, *script = NULL, *nm_file = NULL, *wav_path = NULL;
    unsigned long raw_frames = 0;
    int fresh = 0, sync_start = 0, native_seed_on = 0;
    uint32_t native_seed = 0;
    unsigned devs[4] = { RETRO_DEVICE_JOYPAD, RETRO_DEVICE_JOYPAD, RETRO_DEVICE_JOYPAD,
                         RETRO_DEVICE_JOYPAD };
    struct retro_system_av_info av;
    struct retro_game_info game;

    if (argc < 3) usage();
    core_path = argv[1];
    content = argv[2];
    snprintf(run_dir, sizeof run_dir, "dc/harness/build/run");
    for (size_t i = 0; i < sizeof pinned_opts / sizeof pinned_opts[0]; i++)
        opt_set(pinned_opts[i][0], pinned_opts[i][1]);
    for (int i = 3; i < argc; i++) {
        const char *a = argv[i];
#define NEXT() (i + 1 < argc ? argv[++i] : (usage(), ""))
        if (!strcmp(a, "--script")) script = NEXT();
        else if (!strcmp(a, "--shots")) snprintf(shots_dir, sizeof shots_dir, "%s", NEXT());
        else if (!strcmp(a, "--run-dir")) snprintf(run_dir, sizeof run_dir, "%s", NEXT());
        else if (!strcmp(a, "--fresh")) fresh = 1;
        else if (!strcmp(a, "--frames")) raw_frames = strtoul(NEXT(), NULL, 0);
        else if (!strcmp(a, "--timeout")) timeout_s = atof(NEXT());
        else if (!strcmp(a, "--mailbox")) mb_addr = (uint32_t)strtoul(NEXT(), NULL, 16);
        else if (!strcmp(a, "--nm")) nm_file = NEXT();
        else if (!strcmp(a, "--wav")) wav_path = NEXT();
        else if (!strcmp(a, "--half")) half_shots = 1;
        else if (!strcmp(a, "--sync")) sync_start = 1;
        else if (!strcmp(a, "--native-seed")) {
            native_seed = (uint32_t)strtoul(NEXT(), NULL, 0);
            native_seed_on = sync_start = 1;
        }
        else if (!strcmp(a, "--console")) test_flags = 0;
        else if (!strcmp(a, "--quiet")) quiet = 1;
        else if (!strcmp(a, "--verbose")) quiet = -1;
        else if (!strcmp(a, "--rtc")) {
            const char *r = NEXT();
            if (!strcmp(r, "now")) rtc_now = 1; else rtc_fixed = strtoll(r, NULL, 0);
        } else if (!strcmp(a, "--opt")) {
            char kv[160], *eq;
            snprintf(kv, sizeof kv, "%s", NEXT());
            if (!(eq = strchr(kv, '='))) usage();
            *eq = 0;
            opt_set(kv, eq + 1);
        } else if (!strcmp(a, "--device")) {
            int p = atoi(NEXT());
            const char *d = NEXT();
            if (p < 1 || p > 4) usage();
            devs[p - 1] = !strcmp(d, "keyboard") ? RETRO_DEVICE_KEYBOARD :
                          !strcmp(d, "mouse") ? RETRO_DEVICE_MOUSE :
                          !strcmp(d, "none") ? RETRO_DEVICE_NONE : RETRO_DEVICE_JOYPAD;
        } else usage();
    }
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    setvbuf(stdout, NULL, _IOLBF, 0);

    /* per-run directories: the BIOS-less core still wants a system dir;
     * VMU images land in system/dc/vmu_save_A1.bin and stay there between
     * runs unless --fresh */
    if (fresh) {
        char cmd[1200];
        if (strchr(run_dir, '\'')) die(2, "bad --run-dir");
        snprintf(cmd, sizeof cmd, "rm -rf '%s/system' '%s/saves'", run_dir, run_dir);
        if (system(cmd) != 0) die(2, "cannot clear %s", run_dir);
    }
    snprintf(sys_dir, sizeof sys_dir, "%s/system", run_dir);
    snprintf(save_dir, sizeof save_dir, "%s/saves", run_dir);
    {
        char dcdir[700];
        snprintf(dcdir, sizeof dcdir, "%s/dc", sys_dir);
        mkdirs(dcdir);
        mkdirs(save_dir);
        mkdirs(shots_dir);
    }

    core.h = dlopen(core_path, RTLD_NOW | RTLD_LOCAL);
    if (!core.h) die(2, "cannot load %s: %s", core_path, dlerror());
#define SYM(n) if (!(*(void **)&core.n = dlsym(core.h, "retro_" #n))) die(2, "core has no retro_" #n)
    SYM(set_environment); SYM(set_video_refresh); SYM(set_audio_sample);
    SYM(set_audio_sample_batch); SYM(set_input_poll); SYM(set_input_state);
    SYM(init); SYM(deinit); SYM(get_system_av_info); SYM(set_controller_port_device);
    SYM(load_game); SYM(unload_game); SYM(run); SYM(get_memory_data); SYM(get_memory_size);

    /* Flycast's RTC adds the host's local-time offset to time(); pin TZ so
       the Dreamcast clock is the same on every machine. */
    setenv("TZ", "UTC0", 1);
    tzset();
    core.set_environment(environment);
    core.init();
    core.set_video_refresh(video_refresh);
    core.set_audio_sample(audio_sample);
    core.set_audio_sample_batch(audio_batch);
    core.set_input_poll(input_poll);
    core.set_input_state(input_state);

    memset(&game, 0, sizeof game);
    game.path = content;
    if (!core.load_game(&game)) die(2, "the core cannot load %s", content);
    core.get_system_av_info(&av);
    sample_rate = av.timing.sample_rate;
    if (av.geometry.max_width) fb_max_w = av.geometry.max_width;
    if (av.geometry.max_height) fb_max_h = av.geometry.max_height;
    if (hw_on) {
        gl_setup();
        if (hw.context_reset) hw.context_reset();
    }
    for (int i = 0; i < 4; i++) set_device(i, devs[i]);
    if (wav_path) {
        if (!(wav = fopen(wav_path, "wb"))) die(2, "cannot write %s", wav_path);
        wav_header();
    }
    locate_mailbox(content, nm_file);
    mb_attach();
    if (!quiet)
        printf("dcrun: %s, %ux%u max, %.2f fps, %.0f Hz, %s, mailbox %s\n", content,
               fb_max_w, fb_max_h, av.timing.fps, av.timing.sample_rate,
               hw_on ? "GL" : "software", mb_addr ? "found" : "none");

    if (raw_frames)
        for (unsigned long i = 0; i < raw_frames; i++) run_frame();
    if (script) {
        FILE *f = fopen(script, "r");
        if (!f) die(2, "cannot read %s", script);
        /* --sync: hold the game before its first tick, where the native
         * runner starts a script */
        if (!sync_start || (sync_on(0) &&
                            (!native_seed_on || mb_cmd(DC_HC_NATIVE_SEED, native_seed, NULL, NULL))))
            run_script(f);
        fclose(f);
    }
    sync_off();
    if (mb && !mb_up) fail("the game never wrote the mailbox magic");

    if (wav) { wav_header(); fclose(wav); }
    printf("dcrun: %lu video frames, tick %lu, %.1f s, %d failure%s\n", frames_run,
           tick_now(), elapsed(), fails, fails == 1 ? "" : "s");
    if (mb && mb_up)                    /* DC3: the game's own timing and 2D layer counters */
        printf("dcrun: game drew %u of %u ticks, work mean %u us max %u us; 2D prims max %u, "
               "vertex bytes max %u, dropped %u, texture uploads %u, evictions %u, cached %u\n",
               mb->drawn, mb->ticks, mb->work_n ? mb->work_sum / mb->work_n : 0u, mb->work_max,
               mb->gfx_prims_max, mb->gfx_vbytes_max, mb->gfx_dropped, mb->gfx_uploads,
               mb->gfx_evictions, mb->gfx_tex_live);
    if (mb && mb_up && mb->snd_pushed)  /* DC5: the sound mixer and stream */
        printf("dcrun: sound slots %u played of %u, ring max %u, underruns %u, overruns %u, "
               "dropped %u, gap frames %u, content frames %u, clips %u, effects %u, hash600 0x%08x, "
               "mixer mean %u us max %u us per tick (%u ticks), refill gap max %u us, late refills %u\n",
               mb->snd_consumed, mb->snd_pushed, mb->snd_queue_max, mb->snd_underruns,
               mb->snd_overruns, mb->snd_dropped, mb->snd_gap_frames, mb->snd_frames,
               mb->snd_clips, mb->snd_started, mb->snd_hash600, mb->snd_us_n ? mb->snd_us_sum / mb->snd_us_n : 0u,
               mb->snd_us_max, mb->snd_us_n, mb->snd_cb_gap_max, mb->snd_starved);
    if (mb && mb_up && mb->g3d_n)       /* DC7: the 3D views */
        printf("dcrun: 3D views %u built, build mean %u us max %u us; polygons last %u max %u, "
               "TA bytes max %u, polygons left out %u\n",
               mb->g3d_n, mb->g3d_us_sum / mb->g3d_n, mb->g3d_us_max, mb->g3d_polys,
               mb->g3d_polys_max, mb->g3d_bytes_max, mb->g3d_dropped);
    if (mb && mb_up && wk_ticks)        /* DC3: per tick against the 30 fps budget */
        printf("dcrun: work over %u us on %lu of %lu ticks; worst %u us at tick %u (%s)\n",
               TICK_BUDGET_US, wk_over, wk_ticks, wk_worst, wk_worst_tick, wk_worst_scene);
    fflush(stdout);
    core.unload_game();
    core.deinit();
    return fails ? 1 : 0;
}
