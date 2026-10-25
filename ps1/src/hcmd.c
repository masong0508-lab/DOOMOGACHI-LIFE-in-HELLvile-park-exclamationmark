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
/* --- H3: the story / house runner commands (SPEC R20, C2, C18) --- */
#if !defined(DOOMO_ENGINE_TEST) && defined(DOOMO_HAVE_CORE)
#include "../../core/src/ui_internal.h"     /* UiDialogDebug (expect-text) */
extern int story_flag(int f) WEAKFN;
extern uint32_t story_fingerprint(void) WEAKFN;
extern void ui_dialog_debug(UiDialogDebug *o) WEAKFN;
#endif
/* `seed N` pins plat_entropy() (main.c) so the next town_new() is the
 * native runner's; `room-test N` swaps the house test layouts (room.c) */
uint8_t  g_entropy_pinned;
uint32_t g_entropy_pin;
volatile uint8_t g_room_test;
/* --- end H3 --- */

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
    /* --- H3: story / house checks (SPEC R20, C2, C18); the native
     *     runner's semantics (core/native/runner_main.c) --- */
    } else if (!strcmp(c, "seed")) {
        doomo_town.seed = (uint32_t)n;
        rng_seed(&doomo_rng, (uint32_t)n);
        g_entropy_pin = (uint32_t)n;
        g_entropy_pinned = 1;
        reply(1, "seed", a);
    } else if (!strcmp(c, "expect-flag")) {
        /* iarg = the flag, arg = the wanted value (default 1) */
        int want = a[0] ? (a[0] != '0') : 1;
        int have = HAVE(story_flag) && n >= 0 && n <= 255 ? story_flag(n) != 0 : -1;
        reply(have == want, have > 0 ? "flag is 1" : have == 0 ? "flag is 0" : "no such flag", NULL);
    } else if (!strcmp(c, "expect-text")) {
        UiDialogDebug d;
        memset(&d, 0, sizeof(d));
        if (HAVE(ui_dialog_debug)) ui_dialog_debug(&d);
        reply(d.text && strstr(d.text, a) != NULL, "says", d.text && d.text[0] ? d.text : "(nothing)");
    } else if (!strcmp(c, "story-fp")) {
        /* arg = the expected fingerprint in hex, or empty to print it */
        uint32_t fp = HAVE(story_fingerprint) ? story_fingerprint() : 0, want = 0;
        char buf[12];
        const char *p;
        int i;
        for (p = a; *p; p++) {
            int d = *p >= '0' && *p <= '9' ? *p - '0' : (*p | 32) >= 'a' && (*p | 32) <= 'f' ? (*p | 32) - 'a' + 10 : -1;
            if (d < 0) break;
            want = (want << 4) | (uint32_t)d;
        }
        for (i = 0; i < 8; i++) buf[i] = "0123456789abcdef"[(fp >> (28 - 4 * i)) & 15];
        buf[8] = 0;
        dbg("story-fp: 0x%s\n", buf);
        reply(!a[0] || fp == want, "story-fp", buf);
    } else if (!strcmp(c, "room-test")) {
        g_room_test = (uint8_t)n;
        reply(1, "room-test", a);
    /* --- end H3 --- */
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
