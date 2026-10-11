/* hcmd.c - test-harness mailbox (owner: ps1).
 *
 * Lets ps1/harness run the core native runner's script commands that are
 * more than button presses (scene, push, expect-scene, save, load,
 * expect-save, type, log) on the real PS1 build. The harness Lua writes a
 * request into g_hcmd in emulator RAM between frames and bumps `req`; the
 * main loop calls hcmd_poll() before each game frame, runs the command
 * between two doomo_frame() calls (exactly where the native runner runs
 * it), writes the result and sets `ack = req`.
 *
 * Nothing touches g_hcmd on a real console, so the mailbox is inert there
 * (one compare per frame). Every core symbol is referenced weakly, the
 * mailbox also works (and fails cleanly) in bring-up builds.
 */
#include <string.h>
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
 *   0 magic, 4 req, 8 ack, 12 status (1 ok / 0 fail), 16 iarg,
 *   20 cmd[16], 36 arg[64], 100 reply[64] */
typedef struct {
    uint32_t magic;
    volatile uint32_t req;
    volatile uint32_t ack;
    int32_t status;
    int32_t iarg;
    char cmd[16];
    char arg[64];
    char reply[64];
} HarnessMailbox;

HarnessMailbox g_hcmd = { HCMD_MAGIC, 0, 0, 0, 0, "", "", "" };

static int s_typing;        /* index into arg while `type` feeds chars */

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

static void run(void)
{
    const char *c = g_hcmd.cmd, *a = g_hcmd.arg;
    int n = g_hcmd.iarg;

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
    } else if (!strcmp(c, "log")) {
        dbg("log: %s\n", a);
        reply(1, NULL, NULL);
    } else {
        reply(0, "unknown command", c);
    }
}

void hcmd_poll(void)
{
    if (g_hcmd.req == g_hcmd.ack)
        return;
    if (!strcmp(g_hcmd.cmd, "type")) {
        /* one character per game frame, like the native runner */
        char ch = g_hcmd.arg[s_typing];
        if (!ch) {
            s_typing = 0;
            reply(1, "typed", NULL);
            return;
        }
        if (!HAVE(ui_text_entry_feed) || !ui_text_entry_feed((unsigned char)ch)) {
            s_typing = 0;
            reply(0, "no text entry open for", g_hcmd.arg);
            return;
        }
        s_typing++;
        return;
    }
    run();
}
