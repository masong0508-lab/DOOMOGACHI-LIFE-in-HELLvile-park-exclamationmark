/* hcmd.c - test-harness mailbox and lockstep (owner: ps1).
 *
 * Lets ps1/harness run the core native runner's scripts on the real PS1
 * build with the SAME frame-exact semantics:
 *
 *  - Lockstep. Once the harness Lua sets `lock`, the main loop stops at
 *    the top of the loop (between two game frames, before the pad is read)
 *    whenever the loop counter reaches `stop_at`, waits for the GPU and
 *    publishes `paused` and the VRAM row of the frame just drawn. The
 *    harness then applies pad changes, grabs screenshots of exactly that
 *    frame and runs mailbox commands, and moves `stop_at` on. Between
 *    stops the game runs at full speed (frame rates stay measurable).
 *  - Commands. scene, push, expect-scene, save, load, expect-save, type
 *    (one character), log: the harness writes `cmd`/`arg`/`iarg`, bumps
 *    `req`; hcmd_poll() runs it between frames - exactly where the native
 *    runner runs it - and sets `status`, `reply` and `ack = req`.
 *
 * Nothing touches g_hcmd on a real console, so it costs one compare per
 * frame there. Core symbols are referenced weakly, so the mailbox also
 * works (and fails cleanly) in bring-up builds.
 */
#include <string.h>
#include <psxapi.h>
#include <psxgpu.h>
#include "ps1.h"

#define WEAKFN __attribute__((weak, long_call))

extern const DoomoScene *scene_by_name(const char *name) WEAKFN;
extern void scene_set(const DoomoScene *s, int arg) WEAKFN;
extern void scene_push(const DoomoScene *s, int arg) WEAKFN;
extern const DoomoScene *scene_top(void) WEAKFN;
extern int save_game(int slot) WEAKFN;
extern int load_game(int slot) WEAKFN;
extern int save_exists(int slot) WEAKFN;
extern const char *save_last_error(void) WEAKFN;
extern int ui_text_entry_feed(int c) WEAKFN;

#define HAVE(sym) ({ const void *p_ = (const void *)&(sym); \
                     __asm__("" : "+r"(p_)); p_ != NULL; })

#define HCMD_MAGIC 0x444d4348u   /* "HCMD" */

/* Layout is read by harness.lua (offsets in bytes):
 *   0 magic, 4 req, 8 ack, 12 status (1 ok / 0 fail), 16 iarg, 20 lock,
 *   24 stop_at, 28 paused (loop counter + 1 while stopped, else 0),
 *   32 shot_y, 36 cmd[16], 52 arg[64], 116 reply[64] */
typedef struct {
    uint32_t magic;
    volatile uint32_t req;
    volatile uint32_t ack;
    int32_t status;
    int32_t iarg;
    volatile uint32_t lock;
    volatile uint32_t stop_at;
    volatile uint32_t paused;
    uint32_t shot_y;
    char cmd[16];
    char arg[64];
    char reply[64];
} HarnessMailbox;

HarnessMailbox g_hcmd = { HCMD_MAGIC, 0, 0, 0, 0, 0, 0, 0, 0, "", "", "" };

static void reply(int ok, const char *a, const char *b)
{
    size_t n = 0;
    g_hcmd.status = ok;
    g_hcmd.reply[0] = 0;
    if (a) {
        strncpy(g_hcmd.reply, a, sizeof(g_hcmd.reply) - 1);
        g_hcmd.reply[sizeof(g_hcmd.reply) - 1] = 0;
        n = strlen(g_hcmd.reply);
    }
    if (b && n + 1 < sizeof(g_hcmd.reply)) {
        g_hcmd.reply[n++] = ' ';
        strncpy(g_hcmd.reply + n, b, sizeof(g_hcmd.reply) - 1 - n);
        g_hcmd.reply[sizeof(g_hcmd.reply) - 1] = 0;
    }
    g_hcmd.ack = g_hcmd.req;
}

#if !defined(DOOMO_ENGINE_TEST) && defined(DOOMO_HAVE_CORE)
static const char *top_name(void)
{
    const DoomoScene *t = HAVE(scene_top) ? scene_top() : NULL;
    return t && t->name ? t->name : "(none)";
}

static const char *save_err(void)
{
    const char *e = HAVE(save_last_error) ? save_last_error() : NULL;
    return e ? e : "?";
}
#endif

static void run(void)
{
    const char *c = g_hcmd.cmd, *a = g_hcmd.arg;
    int n = g_hcmd.iarg;

#if defined(DOOMO_ENGINE_TEST) || !defined(DOOMO_HAVE_CORE)
    /* The engine test's scripts only use the lockstep and `log`: no
     * references to the game's scenes or saves here, so --gc-sections can
     * drop them from the engine-test disc. */
    (void)a; (void)n;
    if (!strcmp(c, "log")) {
        dbg("log: %s\n", g_hcmd.arg);
        reply(1, NULL, NULL);
    } else {
        reply(0, "not in the engine test:", c);
    }
#else
    if (!strcmp(c, "scene") || !strcmp(c, "push")) {
        const DoomoScene *s = HAVE(scene_by_name) ? scene_by_name(a) : NULL;
        if (!s || !HAVE(scene_set) || !HAVE(scene_push)) {
            reply(0, "unknown scene", a);
            return;
        }
        if (c[0] == 's') scene_set(s, n); else scene_push(s, n);
        reply(1, "top", top_name());
    } else if (!strcmp(c, "expect-scene")) {
        const char *want = a, *have = top_name();
        if (!strncmp(want, "SCENE_", 6)) want += 6;
        reply(strcmp(want, have) == 0, "top", have);
    } else if (!strcmp(c, "save")) {
        int ok = HAVE(save_game) && save_game(n);
        reply(ok, ok ? "saved" : "save failed:", ok ? NULL : save_err());
    } else if (!strcmp(c, "load")) {
        int ok = HAVE(load_game) && load_game(n);
        reply(ok, ok ? "loaded" : "load failed:", ok ? NULL : save_err());
    } else if (!strcmp(c, "expect-save")) {
        int ok = HAVE(save_exists) && save_exists(n);
        reply(ok, ok ? "valid save" : "no valid save", NULL);
    } else if (!strcmp(c, "type")) {
        /* one character; the harness runs a frame after each */
        int ok = HAVE(ui_text_entry_feed) && ui_text_entry_feed((unsigned char)a[0]);
        reply(ok, ok ? "typed" : "no text entry open", NULL);
    } else if (!strcmp(c, "log")) {
        dbg("log: %s\n", a);
        reply(1, NULL, NULL);
    } else {
        reply(0, "unknown command", c);
    }
#endif
}

static void serve(void)
{
    if (g_hcmd.req != g_hcmd.ack)
        run();
}

void hcmd_poll(void)
{
    if (g_hcmd.lock && g_loop_frames >= g_hcmd.stop_at) {
        uint32_t v;
        DrawSync(0);                    /* the frame is complete in VRAM */
        g_hcmd.shot_y = g_last_draw_y;
        g_hcmd.paused = g_loop_frames + 1;
        while (g_hcmd.lock && g_loop_frames >= g_hcmd.stop_at)
            serve();
        g_hcmd.paused = 0;
        /* let the BIOS poll the pad once more, so buttons the harness
         * changed while we were stopped are seen by this frame */
        v = (uint32_t)VSync(-1);
        while ((uint32_t)VSync(-1) == v)
            serve();
    }
    serve();
}
