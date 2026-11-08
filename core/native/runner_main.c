/* runner_main.c - the headless runner: boots the core, plays a scripted
 * input file and writes PNG screenshots (owner: foundation).
 *
 *   runner [--script FILE] [--shots DIR] [--w 240 --h 160] [--frames N]
 *          [--saves DIR] [--seed N] [--zoom N] [--gba-input] [--save-stream N]
 *          [--assert-no-stubs] [--check-prompts] [--pad NAME] [--quiet]
 *          [--verbose] [--wav FILE] [--budgets FILE] [--frame-hash FILE]
 * --check-prompts (C3) fails the run when a prompt bar or ui_hint line was
 * too wide for the screen or named a button the pad lacks.
 * --pad host|gba|ps1|dc (C3) names the buttons as that console's pad does
 * (input.h input_set_pad): dc/harness/ui_compare.sh runs `--pad dc` so the
 * native shots say START+Y for SELECT as the Dreamcast build does.
 * --budgets FILE (M0) the journey budgets (core/tests/journeys/budgets.tsv):
 * journey-end fails when a journey takes more presses or ticks than its row.
 * --wav FILE records the game's audio (22,050 Hz stereo, musrender: the
 * music SPEC's listening renders, core/native/plat_music_native.c).
 * --frame-hash FILE (SP4) writes one line per frame, "FRAME FNV1A", the hash
 * of the whole framebuffer after the frame: two builds that must draw the
 * same pixels (a speed change) are compared frame by frame with cmp.
 *
 * Script lines (one command per line, '#' starts a comment):
 *   wait N              run N frames
 *   press BTN [BTN..]   hold for 1 frame, then 1 frame released
 *   hold BTN N          hold BTN for N frames, then release it
 *   down BTN / up BTN   press and keep holding / release one button
 *   release             release everything
 *   shot NAME           write SHOTS/NAME.png of the last frame
 *   type "text"         type into the active text entry (1 char/frame;
 *                       \n = OK, \b = delete)
 *   expect-scene NAME   fail unless the top scene is NAME
 *   scene NAME [ARG]    scene_set() to NAME (e.g. UI_GALLERY)
 *   push NAME [ARG]     scene_push()
 *   save SLOT / load SLOT / expect-save SLOT
 *   stick LX LY RX RY   (ST1) an analog pad with its sticks there (-128..127,
 *                       +y down) until the next stick line; `stick off` pulls
 *                       it out. The left stick presses the D-pad past 48, as
 *                       the PS1 / Dreamcast pad drivers do.
 *   log TEXT            print TEXT
 *   no-stubs            fail now if any stub has run
 *   seed N              (S2, SPEC C18) doomo_town.seed = N, and N is what
 *                       plat_entropy() returns from now (the next new town)
 *   expect-text "TEXT"  fail unless the dialogue box's line contains TEXT
 *   expect-toast "TEXT" / expect-no-toast "TEXT"  (CL1) a queued toast says
 *                       TEXT / none does
 *   toasts [reset]      (CL1) print the toasts shown per kind
 *   expect-toasts KIND MAX  (CL1) at most MAX toasts of KIND (unlock hint
 *                       feedback system all) shown so far
 *   hud-watch on|off    (CL1) fail if a toast ever covers the story HUD band
 *   expect-flag F [V]   fail unless story flag F (0..255) is V (default 1)
 *   story-fp [HEX]      print story_fingerprint(); with HEX, fail unless equal
 *   side M [STATE]      (S4) set side mission S<M>'s state (default 3, done)
 *   talk ID             (S4) talk to resident ID (story_interact_npc)
 *   cell X Y            (S4) as if the player stepped on cell X,Y (a find verb)
 *   skip-talk [MAX]     (S4) press A until the conversation is over
 *   skip-to "TEXT" [MAX]      (S4) press A until the dialogue says TEXT
 *   skip-to-scene NAME [MAX]  (S4) press A until NAME is the top scene
 *   home-room R         (H4) move in and open cottage room R (0..5), as the
 *                       story / HomeMart would
 *   clock H M           (H4) the island clock (the house's daylight)
 *   money N             (H4) the purse (HomeMart's deals)
 *   lang CODE           (P0) the language by its code (en, es, pt-BR...:
 *                       core/lang/languages.tsv, ready or not), as if picked
 *                       on the Options page: opt_set(OPT_LANGUAGE) ->
 *                       lang_changed(); the session keeps it across loads
 *   expect-lang CODE    (P0) fail unless lang_get() is that language
 *   opt ID VALUE        (SP8) opt_set(ID, VALUE) for an OPT_* id (town.h), e.g.
 *                       63 1 = Combat detail: Smooth; fails unless opt_get reads it
 *   journey-begin NAME  (M0) start the journey meter (alias: mark NAME)
 *   journey-end [NAME]  (M0) stop it and print "journey: NAME presses=P
 *                       ticks=T depth=D screens=S typed=C"; with --budgets
 *                       fail over that row's presses or ticks (alias:
 *                       report [NAME])
 *   expect-presses NAME [<=] N  (M0) fail unless journey NAME took <= N
 *                       presses
 *   problem WHO KIND [ITEM]  (M0) sim_problem_raise() for the citizen named
 *                       WHO; KIND hungry/dirty/bored/sick/item/fight/lonely
 *                       or a PROB_* number, ITEM an item name or number
 *   item ITEM [N]       (M0) inv_add(ITEM, N) (default 1; the bag)
 *   expect-item ITEM N  (M0) fail unless the bag holds exactly N of ITEM
 *   expect-problem WHO [0|1]  (M0) fail unless WHO has (1, default) or has
 *                       no (0) open problem
 *   expect-money MIN    (M0) fail unless the purse holds at least MIN H$
 *   expect-rel WHO OTHER [0|1]  (M0) fail unless the two know each other
 *                       (1, default: any relationship) or do not (0)
 *   first-boot          (CL2) play the real first boot: from now on the
 *                       runner keeps a machine config record like a console
 *                       (in memory, PLATF_CONFIG), the record is wiped, the
 *                       settings are reloaded and BOOT starts, so the
 *                       language picker, the Mature question and the
 *                       security code come up as on a new console
 *   expect-hero NAME    (CL2) fail unless the protagonist (the save's hero,
 *                       town_res(DOOMO_PLAYER_ID)) is called NAME
 * Buttons: UP DOWN LEFT RIGHT A B L R START SELECT X Y L2 R2.
 * Exit status: 0 ok, 1 script failure, 2 stubs ran with --assert-no-stubs.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include "native.h"
#include "plat_music_native.h"            /* music: --wav (musrender) */
#include "doomo/story_vm.h"           /* S2: story_fingerprint */
#include "doomo/music.h"              /* music: music-hash */
#include "../src/ui_internal.h"       /* S2: ui_dialog_debug (expect-text) */
#include "doomo/lang.h"               /* P0: lang CODE */
#include "doomo/tips.h"               /* CL4: tips on, expect-tip */
#include "doomo/items.h"              /* M0: item, problem (journey setups) */
#include "doomo/sim.h"
#include "doomo/plat_ext.h"           /* CL2: first-boot (the config record) */
#include "doomo/options_store.h"
void story_verbs_cell(int cx, int cy);    /* S4: "cell", "skip-talk" (core/src/story_int.h) */
int  story_talking(void);
void home_move_in(void);                  /* H4: "home-room" (core/src/home_int.h) */
int  owfx_hud_band(int *obj_x, int *obj_y, int *obj_w, int *obj_h);   /* CL1: hud-watch (core/src/overworld_int.h) */
void home_unlock_room(int room);

/* --- CL2 (C17): a console's machine config record, for "first-boot" ---
 * Off until a script asks: every other script keeps running with no
 * record (plat_features() 0, as before), so nothing else changes. */
static int cfg_on;
static uint8_t cfg_rec[PLAT_CONFIG_MAX];
static uint32_t cfg_len;

uint32_t plat_features(void) { return cfg_on ? PLATF_CONFIG : 0u; }

int plat_config_read(uint8_t *dst, uint32_t cap)
{
    uint32_t n = cfg_len < cap ? cfg_len : cap;
    if (!cfg_on || !dst) return 0;
    memcpy(dst, cfg_rec, n);
    return (int)n;
}

int plat_config_write(const uint8_t *src, uint32_t len)
{
    if (!cfg_on || !src || len > PLAT_CONFIG_MAX) return 0;
    memcpy(cfg_rec, src, len);
    cfg_len = len;
    return 1;
}
/* --- end CL2 --- */

static uint32_t held;
static int gba_input;
static const char *shots_dir = ".";
static int zoom = 1;
static int failures;
static const char *script_name = "(no script)";
static int line_no;
static FILE *frame_hash;                  /* SP4: --frame-hash FILE */
static uint32_t frame_no;

/* ui_raster.c (ui_internal.h): the UI shape cache's counters */
void ur_cache_stats(int *used, int *entries, uint32_t *hits, uint32_t *misses,
                    uint32_t *resets);
void ur_cache_image_stats(uint32_t *replayed, uint32_t *built, uint32_t *drawn);

/* "stats LABEL": draw calls per frame and UI shape cache activity since
 * the previous "stats" (perf work: what a screen asks of the platform) */
static void stats(const char *label)
{
    static uint32_t c0[NC_N], t0, h0, m0, r0, p0, b0, d0;
    static const char *const nm[NC_N] = { "rect", "text", "image", "checker", "shade",
                                          "line", "clear", "view" };
    uint32_t t = plat_ticks(), n = t - t0 ? t - t0 : 1, hits, misses, resets, rep, blt, drw;
    int used, ents, i;
    ur_cache_stats(&used, &ents, &hits, &misses, &resets);
    ur_cache_image_stats(&rep, &blt, &drw);
    printf("stats %s: %u frames, per frame:", label, (unsigned)(t - t0));
    for (i = 0; i < NC_N; i++) {
        if (nat_calls[i] - c0[i]) printf(" %s %.1f", nm[i], (double)(nat_calls[i] - c0[i]) / n);
        c0[i] = nat_calls[i];
    }
    printf(" | shape cache %d rects %d entries, hits %.1f misses %.1f resets %.2f, "
           "replayed rects %.1f, images built %.2f drawn %.1f\n", used, ents,
           (double)(hits - h0) / n, (double)(misses - m0) / n, (double)(resets - r0) / n,
           (double)(rep - p0) / n, (double)(blt - b0) / n, (double)(drw - d0) / n);
    t0 = t; h0 = hits; m0 = misses; r0 = resets; p0 = rep; b0 = blt; d0 = drw;
}

/* --- M0: the journey meter (core/tests/journeys/, run_journeys.sh) ---
 * Between journey-begin and journey-end every script command that presses
 * a button counts 1 press (press, hold, down, each A of skip-talk /
 * skip-to, the OK "\n" of a typed text), every frame run counts 1 tick (a
 * press is 1 press + 2 ticks) and each typed letter counts 1 typed. depth
 * is the deepest the scene stack went above where the journey began
 * (pushed scenes and modals), screens the number of times the top scene
 * changed. Setup commands that change the game's state without a press
 * (scene, push, money, item, problem, ...: jr_setup_cmd) are refused while
 * a journey runs, so a journey cannot skip presses it would otherwise
 * count; set the state up before journey-begin. */
#define JNAME 48
static struct {
    int on;
    char name[JNAME];
    uint32_t presses, ticks, typed;
    int base, depth, screens;
    const DoomoScene *top;
} jr;
static struct { char name[JNAME]; uint32_t presses; } jdone[64];
static int njdone;
static struct { char name[JNAME]; long presses, ticks; } jbud[128];
static int njbud, have_budgets;

static void jr_frame(void)
{
    const DoomoScene *t = scene_top();
    int d = scene_depth() - jr.base;
    jr.ticks++;
    if (d > jr.depth) jr.depth = d;
    if (t != jr.top) { jr.screens++; jr.top = t; }
}

static int jr_setup_cmd(const char *cmd)
{
    static const char *const setup[] = {
        "scene", "push", "save", "load", "seed", "lang", "story-fp", "side",
        "talk", "cell", "home-room", "clock", "money", "item", "problem",
        "first-boot", "tips",
    };
    size_t k;
    for (k = 0; k < sizeof(setup) / sizeof(setup[0]); k++)
        if (strcmp(cmd, setup[k]) == 0) return 1;
    return 0;
}

static void jr_press(void)
{
    if (jr.on) jr.presses++;
}

/* journey setups: a citizen by name, an item by name, a problem kind */
static int res_named(const char *nm)
{
    int i;
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++)
        if (doomo_town.res[i].status != RES_EMPTY && strcmp(doomo_town.res[i].name, nm) == 0) return i;
    return -1;
}

static int item_named(const char *nm)
{
    int i;
    if (isdigit((unsigned char)nm[0])) return atoi(nm);
    for (i = 1; i < ITEM_COUNT; i++)
        if (doomo_items[i].name && strcmp(doomo_items[i].name, nm) == 0) return i;
    return -1;
}

static int problem_kind(const char *nm)
{
    static const struct { const char *n; int k; } kinds[] = {
        { "hungry", PROB_HUNGRY }, { "dirty", PROB_DIRTY }, { "bored", PROB_BORED },
        { "sick", PROB_SICK }, { "item", PROB_WANTS_ITEM }, { "fight", PROB_FIGHT },
        { "lonely", PROB_LONELY }
    };
    size_t i;
    if (isdigit((unsigned char)nm[0])) return atoi(nm);
    for (i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++)
        if (strcmp(kinds[i].n, nm) == 0) return kinds[i].k;
    return -1;
}

static int load_budgets(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[256];
    if (!f) return 0;
    while (fgets(line, sizeof(line), f) && njbud < (int)(sizeof(jbud) / sizeof(jbud[0]))) {
        char name[JNAME], p[32], t[32];
        if (line[0] == '#' || sscanf(line, "%47s %31s %31s", name, p, t) != 3) continue;
        snprintf(jbud[njbud].name, JNAME, "%s", name);
        jbud[njbud].presses = p[0] == '-' ? -1 : atol(p);   /* "-": reserved row */
        jbud[njbud].ticks = t[0] == '-' ? -1 : atol(t);
        njbud++;
    }
    fclose(f);
    have_budgets = 1;
    return 1;
}
/* --- end M0 --- */

/* --- CL1 (clarity C1): hud-watch. While on, every frame on which the story overworld drew its HUD band and a
 * toast was drawn is checked: the toast must lie under the band, off the objective pill and the clock. --- */
static int hudw_on, hudw_frames, hudw_bad;
static void hudw_frame(void)
{
    int ox, oy, ow_, oh, tx, ty, tw, th, band = owfx_hud_band(&ox, &oy, &ow_, &oh);
    if (!band || !ui_toast_last_rect(&tx, &ty, &tw, &th)) return;
    hudw_frames++;
    if (ty < band || (ow_ > 0 && !(tx + tw <= ox || tx >= ox + ow_ || ty + th <= oy || ty >= oy + oh))) {
        if (!hudw_bad) fprintf(stderr, "hud-watch: frame %u: toast at (%d,%d %dx%d) over the HUD band (bottom %d)\n",
                               (unsigned)doomo_frame_count(), tx, ty, tw, th, band);
        hudw_bad++;
    }
}
/* --- end CL1 --- */

static void step(int n)
{
    while (n-- > 0) {
        plat_begin_frame();
        doomo_frame(held | input_stick_mask() | (gba_input ? DOOMO_INPUT_SYNTH_XY : 0));
        plat_end_frame();
        if (jr.on) jr_frame();   /* M0 */
        if (hudw_on) hudw_frame();   /* CL1 */
        if (frame_hash) {        /* SP4: the frame's pixels, for exact-speed-change checks */
            uint32_t h = 2166136261u;
            int k, n = nat_w * nat_h;
            for (k = 0; k < n; k++) h = (h ^ nat_fb[k]) * 16777619u;
            fprintf(frame_hash, "%u %08x\n", (unsigned)frame_no, (unsigned)h);
        }
        frame_no++;
    }
}

static uint32_t button(const char *s)
{
    static const struct { const char *n; uint32_t b; } names[] = {
        { "UP", BTN_UP }, { "DOWN", BTN_DOWN }, { "LEFT", BTN_LEFT },
        { "RIGHT", BTN_RIGHT }, { "A", BTN_A }, { "B", BTN_B }, { "L", BTN_L },
        { "R", BTN_R }, { "START", BTN_START }, { "SELECT", BTN_SELECT },
        { "X", BTN_X }, { "Y", BTN_Y }, { "L2", BTN_L2 }, { "R2", BTN_R2 },
        { "L3", BTN_L3 }, { "R3", BTN_R3 }
    };
    size_t i;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (strcmp(s, names[i].n) == 0) return names[i].b;
    return 0;
}

static void fail(const char *msg, const char *arg)
{
    fprintf(stderr, "runner: %s:%d: %s %s\n", script_name, line_no, msg, arg ? arg : "");
    failures++;
}

static void mkdirs(const char *dir)
{
    char tmp[512];
    size_t i;
    snprintf(tmp, sizeof(tmp), "%s", dir);
    for (i = 1; tmp[i]; i++) {
        if (tmp[i] == '/') {
            tmp[i] = 0;
            mkdir(tmp, 0777);
            tmp[i] = '/';
        }
    }
    mkdir(tmp, 0777);
}

static void shot(const char *name)
{
    char path[700];
    snprintf(path, sizeof(path), "%s/%s.png", shots_dir, name);
    if (!nat_png_write(path, nat_fb, nat_w, nat_h, zoom)) fail("could not write", path);
    else printf("shot: %s\n", path);
}

static int stub_check(const char *when)
{
    int i, n = nat_stub_count();
    if (!n) return 0;
    fprintf(stderr, "runner: %d stub(s) ran %s:\n", n, when);
    for (i = 0; i < n; i++) fprintf(stderr, "  STUB: %s\n", nat_stub_name(i));
    return n;
}

/* split a line into words; "quoted strings" keep spaces and escapes */
static int split(char *line, char **argv, int max)
{
    int n = 0;
    char *p = line;
    while (*p && n < max) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p || *p == '#') break;
        if (*p == '"') {
            char *w = ++p, *o = p;
            argv[n++] = w;
            while (*p && *p != '"') {
                if (*p == '\\' && p[1]) {
                    p++;
                    *o++ = *p == 'n' ? '\n' : *p == 'b' ? '\b' : *p == 't' ? '\t' : *p;
                    p++;
                } else {
                    *o++ = *p++;
                }
            }
            if (*p == '"') p++;
            *o = 0;
            continue;
        }
        argv[n++] = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        if (*p) *p++ = 0;
    }
    return n;
}

static void run_line(char *line)
{
    char *argv[16];
    int argc = split(line, argv, 16), i;
    const char *cmd;
    if (argc == 0) return;
    cmd = argv[0];
    if (jr.on && jr_setup_cmd(cmd)) {   /* M0: no teleports inside a journey */
        fail("setup command inside a journey (move it before journey-begin):", cmd);
        return;
    }
    if (strcmp(cmd, "wait") == 0) {
        step(argc > 1 ? atoi(argv[1]) : 1);
    } else if (strcmp(cmd, "press") == 0) {
        uint32_t b = 0;
        for (i = 1; i < argc; i++) {
            uint32_t one = button(argv[i]);
            if (!one) fail("unknown button", argv[i]);
            b |= one;
        }
        jr_press();
        held |= b;
        step(1);
        held &= ~b;
        step(1);
    } else if (strcmp(cmd, "hold") == 0) {
        uint32_t b = argc > 1 ? button(argv[1]) : 0;
        if (!b) { fail("unknown button", argc > 1 ? argv[1] : ""); return; }
        jr_press();
        held |= b;
        step(argc > 2 ? atoi(argv[2]) : 1);
        held &= ~b;
    } else if (strcmp(cmd, "down") == 0) {
        uint32_t b = argc > 1 ? button(argv[1]) : 0;
        if (!b) fail("unknown button", argc > 1 ? argv[1] : "");
        else if (!(held & b)) jr_press();
        held |= b;
    } else if (strcmp(cmd, "up") == 0) {
        held &= ~(argc > 1 ? button(argv[1]) : 0u);
    } else if (strcmp(cmd, "release") == 0) {
        if (argc > 1) held &= ~button(argv[1]); else held = 0;
    } else if (strcmp(cmd, "shot") == 0) {
        shot(argc > 1 ? argv[1] : "shot");
    } else if (strcmp(cmd, "type") == 0) {
        const char *s = argc > 1 ? argv[1] : "";
        for (; *s; s++) {
            if (!ui_text_entry_feed((unsigned char)*s)) { fail("no text entry open for", argv[1]); break; }
            if (*s == '\n') jr_press();        /* M0: OK is a press, letters are typed */
            else if (jr.on) jr.typed++;
            step(1);
        }
    } else if (strcmp(cmd, "expect-scene") == 0) {
        const DoomoScene *top = scene_top();
        const char *want = argc > 1 ? argv[1] : "";
        if (strncmp(want, "SCENE_", 6) == 0) want += 6;
        if (!top || strcmp(top->name, want) != 0) {
            char msg[128];
            snprintf(msg, sizeof(msg), "expected scene %s but top is", want);
            fail(msg, top ? top->name : "(none)");
        }
    } else if (strcmp(cmd, "scene") == 0 || strcmp(cmd, "push") == 0) {
        const DoomoScene *s = argc > 1 ? scene_by_name(argv[1]) : NULL;
        if (!s) { fail("unknown scene", argc > 1 ? argv[1] : ""); return; }
        if (cmd[0] == 's') scene_set(s, argc > 2 ? atoi(argv[2]) : 0);
        else scene_push(s, argc > 2 ? atoi(argv[2]) : 0);
    } else if (strcmp(cmd, "save") == 0) {
        if (!save_game(argc > 1 ? atoi(argv[1]) : 1)) fail("save failed:", save_last_error());
    } else if (strcmp(cmd, "load") == 0) {
        if (!load_game(argc > 1 ? atoi(argv[1]) : 1)) fail("load failed:", save_last_error());
    } else if (strcmp(cmd, "expect-save") == 0) {
        if (!save_exists(argc > 1 ? atoi(argv[1]) : 1)) fail("no valid save in slot", argc > 1 ? argv[1] : "1");
    } else if (strcmp(cmd, "log") == 0) {
        printf("log:");
        for (i = 1; i < argc; i++) printf(" %s", argv[i]);
        printf("\n");
    } else if (strcmp(cmd, "stats") == 0) {
        stats(argc > 1 ? argv[1] : "");
    } else if (strcmp(cmd, "no-stubs") == 0) {
        if (stub_check("so far")) failures++;
    /* --- S2: story checks (SPEC R20 / C2: seed, expect-text, expect-flag, story-fp) --- */
    } else if (strcmp(cmd, "seed") == 0) {
        uint32_t v = argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 0) : 0;
        doomo_town.seed = v;
        nat_set_entropy(v);
    /* --- P0: lang CODE. Codes, not numbers, so scripts survive the language
     * registry; L1: any code of core/lang/languages.tsv (lang_find), ready
     * or not. --- */
    } else if (strcmp(cmd, "stick") == 0) {                        /* ST1 */
        if (argc > 1 && strcmp(argv[1], "off") == 0) input_set_analog(0, 0, 0, 0, 0, 0);
        else if (argc < 5) fail("stick needs LX LY RX RY (or off), got", argc > 1 ? argv[1] : "");
        else {
            int lx = atoi(argv[1]), ly = atoi(argv[2]);
            input_set_analog(1, lx, ly, atoi(argv[3]), atoi(argv[4]), input_stick_dpad(lx, ly));
        }
    } else if (strcmp(cmd, "opt") == 0) {                          /* SP8 */
        int id = argc > 1 ? atoi(argv[1]) : -1, v = argc > 2 ? atoi(argv[2]) : 0;
        if (argc < 3 || id < 0) fail("opt needs an option id and a value, got", argc > 1 ? argv[1] : "");
        else {
            opt_set(id, v);
            if (opt_get(id) != v) fail("opt did not take the value for id", argv[1]);
        }
    } else if (strcmp(cmd, "lang") == 0 || strcmp(cmd, "expect-lang") == 0) {
        const char *want = argc > 1 ? argv[1] : "";
        int found = lang_find(want), now;
        if (found < 0) fail("lang needs a code of core/lang/languages.tsv (en, es, pt-BR...), got", want);
        else if (cmd[0] == 'l') opt_set(OPT_LANGUAGE, found);
        else if ((now = lang_get()) != found) {
            char msg[64];
            snprintf(msg, sizeof(msg), "expected the language to be %s but it is", want);
            fail(msg, doomo_lang_info[now].code);
        }
    /* --- end P0 --- */
    } else if (strcmp(cmd, "first-boot") == 0) {          /* CL2 (C17) */
        cfg_on = 1;
        cfg_len = 0;
        memset(cfg_rec, 0, sizeof(cfg_rec));
        opt_config_load();                                 /* no record: first boot */
        if (!opt_config_first_boot()) fail("first-boot: the settings did not see a first boot", "");
        scene_set(scene_by_name("BOOT"), 0);
    } else if (strcmp(cmd, "expect-hero") == 0) {         /* CL2 (C2) */
        const DoomoResident *p = town_res(DOOMO_PLAYER_ID);
        const char *want = argc > 1 ? argv[1] : "";
        if (!p || p->status == RES_EMPTY || strcmp(p->name, want) != 0) {
            char msg[96];
            snprintf(msg, sizeof(msg), "expected the hero to be %s but it is", want);
            fail(msg, p && p->status != RES_EMPTY ? p->name : "(nobody)");
        }
    /* --- CL1 (clarity C1 + C5): toast checks ---
     *   expect-toast "TEXT" / expect-no-toast "TEXT"   a queued toast (or the one on screen) says TEXT, or none does
     *   toasts [reset]       print the toasts shown so far per kind (reset: start counting again)
     *   expect-toasts KIND MAX   fail if more than MAX toasts of KIND (unlock hint feedback system all) were shown
     *   hud-watch on|off     check every frame that a toast never covers the story HUD band (off prints the tally) */
    } else if (strcmp(cmd, "expect-toast") == 0 || strcmp(cmd, "expect-no-toast") == 0) {
        const char *want = argc > 1 ? argv[1] : "";
        int k, found = 0;
        for (k = 0; ui_toast_text(k); k++) if (strstr(ui_toast_text(k), want)) found = 1;
        if (found != (cmd[7] == 't')) fail(cmd[7] == 't' ? "no toast says" : "a toast still says", want);
    } else if (strcmp(cmd, "toasts") == 0) {
        printf("toasts: unlock=%d hint=%d feedback=%d system=%d news-waiting=%d\n", ui_toast_shown(UI_TOAST_UNLOCK),
               ui_toast_shown(UI_TOAST_HINT), ui_toast_shown(UI_TOAST_FEEDBACK), ui_toast_shown(UI_TOAST_SYSTEM),
               ui_toast_news_waiting());
        if (argc > 1 && strcmp(argv[1], "reset") == 0) ui_toast_shown_reset();
    } else if (strcmp(cmd, "expect-toasts") == 0) {
        static const char *const kinds[] = { "unlock", "hint", "feedback", "system", "all" };
        int k, max = argc > 2 ? atoi(argv[2]) : 0;
        for (k = 0; k < 5 && !(argc > 1 && strcmp(argv[1], kinds[k]) == 0); k++) {}
        if (k == 5) fail("expect-toasts needs unlock, hint, feedback, system or all, got", argc > 1 ? argv[1] : "");
        else if (ui_toast_shown(k < 4 ? k : -1) > max) {
            char msg[96];
            snprintf(msg, sizeof(msg), "expected at most %d %s toasts but %d were shown; max", max, kinds[k],
                     ui_toast_shown(k < 4 ? k : -1));
            fail(msg, argv[2]);
        }
    } else if (strcmp(cmd, "hud-watch") == 0) {
        if (argc > 1 && strcmp(argv[1], "off") == 0) {
            printf("hud-watch: %d frames with a toast over the story HUD, %d covered it\n", hudw_frames, hudw_bad);
            if (hudw_bad) fail("a toast covered the story HUD band on frames:", "(see hud-watch above)");
            hudw_on = 0;
        } else { hudw_on = 1; hudw_frames = hudw_bad = 0; }
    /* --- end CL1 --- */
    /* --- CL4: first-time tips. "tips on" lets the cards show in this run (the runner keeps them off,
     * plat_test_flags); "tips reset" forgets the seen ones; "expect-tip NAME" (tips.c tip_name, or
     * "none") checks the card on screen. --- */
    } else if (strcmp(cmd, "tips") == 0) {
        const char *a1 = argc > 1 ? argv[1] : "";
        if (strcmp(a1, "on") == 0) tips_test_force(1);
        else if (strcmp(a1, "off") == 0) tips_test_force(0);
        else if (strcmp(a1, "reset") == 0) tips_reset();
        else fail("tips needs on, off or reset, got", a1);
    } else if (strcmp(cmd, "expect-tip") == 0) {
        const char *want = argc > 1 ? argv[1] : "none";
        int cur = tip_active() ? tip_current() : -1;
        const char *now = cur >= 0 ? tip_name(cur) : "none";
        if (strcmp(now, want) != 0) {
            char msg[64];
            snprintf(msg, sizeof(msg), "expected the tip card %s but it is", want);
            fail(msg, now);
        }
    /* --- end CL4 --- */
    } else if (strcmp(cmd, "expect-text") == 0) {
        UiDialogDebug d;
        const char *want = argc > 1 ? argv[1] : "";
        memset(&d, 0, sizeof(d));
        ui_dialog_debug(&d);
        if (!d.text || !strstr(d.text, want)) {
            char msg[256];
            snprintf(msg, sizeof(msg), "expected the dialogue to say \"%s\" but it says", want);
            fail(msg, d.text && d.text[0] ? d.text : "(nothing)");
        }
    } else if (strcmp(cmd, "expect-flag") == 0) {
        int f = argc > 1 ? atoi(argv[1]) : -1, want = argc > 2 ? atoi(argv[2]) : 1;
        if (f < 0 || f > 255) fail("expect-flag needs a flag 0..255, got", argc > 1 ? argv[1] : "");
        else if (story_flag(f) != (want != 0)) {
            char msg[64];
            snprintf(msg, sizeof(msg), "expected story flag %d to be %d but it is", f, want != 0);
            fail(msg, story_flag(f) ? "1" : "0");
        }
    } else if (strcmp(cmd, "story-done") == 0) {
        /* PA10 part 3: make this town a finished story (save it with `save 0` for the title to find) */
        doomo_town.mode = TOWN_STORY;
        doomo_town.story.chapter = STORY_CH_DONE;
    } else if (strcmp(cmd, "ending-seen") == 0) {
        /* PA10 part 4 (FREE): reach an ending, which opens play-as in Free play (the unlock outlives the world) */
        unlock_grant_quiet(UL_STORY_END(0));
    } else if (strcmp(cmd, "story-fp") == 0) {
        uint32_t fp = story_fingerprint();
        printf("story-fp: 0x%08x\n", (unsigned)fp);
        if (argc > 1 && (uint32_t)strtoul(argv[1], NULL, 16) != fp) fail("story fingerprint differs from", argv[1]);
    /* --- end S2 --- */
    /* --- S4: side missions (side_sample_a.txt) ---
     *   side M STATE   set mission S<M>'s state (0 none .. 3 done), e.g. a
     *                  chain's earlier parts
     *   talk ID        talk to resident ID as if you pressed A in front of them */
    } else if (strcmp(cmd, "side") == 0) {
        int m = argc > 1 ? atoi(argv[1]) : 0;
        if (m < 100 || m >= 100 + STORY_SIDE2_ROWS) fail("side needs a mission 100..217, got", argc > 1 ? argv[1] : "");
        else story_side2_set_state(m - 100, argc > 2 ? atoi(argv[2]) : 3);
    } else if (strcmp(cmd, "talk") == 0) {
        story_interact_npc(argc > 1 ? atoi(argv[1]) : -1);
    } else if (strcmp(cmd, "skip-talk") == 0) {
        /* press A until the conversation is over and the overworld is back */
        int max = argc > 1 ? atoi(argv[1]) : 120, k;
        for (k = 0; (story_talking() || scene_top() != &SCENE_STORY) && k < max; k++) {
            jr_press();
            held |= BTN_A;
            step(1);
            held &= ~(uint32_t)BTN_A;
            step(10);
        }
        if (story_talking() || scene_top() != &SCENE_STORY) fail("the conversation never ended", "");
    } else if (strcmp(cmd, "cell") == 0) {
        story_verbs_cell(argc > 1 ? atoi(argv[1]) : 0, argc > 2 ? atoi(argv[2]) : 0);
    } else if (strcmp(cmd, "skip-to") == 0 || strcmp(cmd, "skip-to-scene") == 0) {
        /* press A (then wait 10 frames) until the dialogue says TEXT / the
         * top scene is NAME; fail after MAX presses (default 80) */
        const char *want = argc > 1 ? argv[1] : "";
        int max = argc > 2 ? atoi(argv[2]) : 80, k, scene = cmd[7] == '-';
        if (scene && strncmp(want, "SCENE_", 6) == 0) want += 6;
        for (k = 0; k <= max; k++) {
            UiDialogDebug d;
            memset(&d, 0, sizeof(d));
            ui_dialog_debug(&d);
            if (scene ? (scene_top() && strcmp(scene_top()->name, want) == 0) : (d.text && strstr(d.text, want) != NULL)) break;
            if (k == max) { fail(scene ? "never reached the scene" : "the dialogue never said", want); break; }
            jr_press();
            held |= BTN_A;
            step(1);
            held &= ~(uint32_t)BTN_A;
            step(10);
        }
    /* --- end S4 --- */
    /* --- H4: the house's runner scripts (home_rooms.txt, home_mart.txt) --- */
    } else if (strcmp(cmd, "home-room") == 0) {
        home_move_in();
        home_unlock_room(argc > 1 ? atoi(argv[1]) : 0);
    } else if (strcmp(cmd, "clock") == 0) {
        int hh = argc > 1 ? atoi(argv[1]) : 8, mm = argc > 2 ? atoi(argv[2]) : 0;
        doomo_town.minute = (uint16_t)(((hh * 60 + mm) % 1440 + 1440) % 1440);
    } else if (strcmp(cmd, "money") == 0) {
        doomo_town.money = (uint32_t)strtoul(argc > 1 ? argv[1] : "0", NULL, 0);
    /* --- end H4 --- */
    /* --- music: render and hash commands (MU1, SPEC sec.16.3, row 39) ---
     *   music N | music-once N | music-dual A B   audio_music*() by SONG_* number
     *   mute N | duck Q8 N | tapestop N | resume  S1's comic-timing hooks
     *   music-hash CUE MIN                         print music_hash() (T2) */
    } else if (strcmp(cmd, "music") == 0) {
        audio_music(argc > 1 ? atoi(argv[1]) : SONG_NONE);
    } else if (strcmp(cmd, "music-once") == 0) {
        audio_music_once(argc > 1 ? atoi(argv[1]) : SONG_NONE);
    } else if (strcmp(cmd, "music-dual") == 0) {
        audio_music_dual(argc > 1 ? atoi(argv[1]) : SONG_NONE, argc > 2 ? atoi(argv[2]) : SONG_NONE);
    } else if (strcmp(cmd, "mute") == 0) {
        audio_music_mute(argc > 1 ? atoi(argv[1]) : 0);
    } else if (strcmp(cmd, "duck") == 0) {
        audio_music_duck(argc > 1 ? atoi(argv[1]) : 256, argc > 2 ? atoi(argv[2]) : 0);
    } else if (strcmp(cmd, "tapestop") == 0) {
        audio_tapestop(argc > 1 ? atoi(argv[1]) : 5);
    } else if (strcmp(cmd, "resume") == 0) {
        audio_music_resume_on_beat();
    } else if (strcmp(cmd, "music-hash") == 0) {
        int cue = argc > 1 ? atoi(argv[1]) : 0, min = argc > 2 ? atoi(argv[2]) : 2;
        printf("music-hash %d %d: 0x%08x\n", cue, min, (unsigned)music_hash(cue, min));
    /* --- end music --- */
    /* --- M0: the journey meter (see jr above; core/tests/journeys/) --- */
    } else if (strcmp(cmd, "journey-begin") == 0 || strcmp(cmd, "mark") == 0) {
        if (jr.on) { fail("journey already running:", jr.name); return; }
        memset(&jr, 0, sizeof(jr));
        snprintf(jr.name, JNAME, "%s", argc > 1 ? argv[1] : "journey");
        jr.on = 1;
        jr.base = scene_depth();
        jr.top = scene_top();
    } else if (strcmp(cmd, "journey-end") == 0 || strcmp(cmd, "report") == 0) {
        int k;
        if (!jr.on) { fail("no journey running for", argc > 1 ? argv[1] : cmd); return; }
        if (argc > 1 && strcmp(argv[1], jr.name) != 0) fail("journey-end does not match the running journey", jr.name);
        jr.on = 0;
        printf("journey: %s presses=%u ticks=%u depth=%d screens=%d typed=%u\n", jr.name,
               (unsigned)jr.presses, (unsigned)jr.ticks, jr.depth, jr.screens, (unsigned)jr.typed);
        if (njdone < (int)(sizeof(jdone) / sizeof(jdone[0]))) {
            snprintf(jdone[njdone].name, JNAME, "%s", jr.name);
            jdone[njdone++].presses = jr.presses;
        }
        if (!have_budgets) return;
        for (k = 0; k < njbud && strcmp(jbud[k].name, jr.name) != 0; k++) {}
        if (k == njbud || jbud[k].presses < 0) { fail("no budget row (budgets.tsv) for journey", jr.name); return; }
        if ((long)jr.presses > jbud[k].presses || (jbud[k].ticks >= 0 && (long)jr.ticks > jbud[k].ticks)) {
            char msg[160];
            snprintf(msg, sizeof(msg), "over budget: %u presses / %u ticks, the budget is %ld / %ld, journey",
                     (unsigned)jr.presses, (unsigned)jr.ticks, jbud[k].presses, jbud[k].ticks);
            fail(msg, jr.name);
        }
    } else if (strcmp(cmd, "expect-presses") == 0) {
        const char *nm = argc > 1 ? argv[1] : "";
        long max = argc > 2 ? atol(argv[argc - 1]) : -1;
        int k;
        long got = -1;
        if (jr.on && strcmp(jr.name, nm) == 0) got = (long)jr.presses;
        for (k = njdone - 1; got < 0 && k >= 0; k--)
            if (strcmp(jdone[k].name, nm) == 0) got = (long)jdone[k].presses;
        if (got < 0 || max < 0) fail("expect-presses needs a measured journey and a count, got", nm);
        else if (got > max) {
            char msg[96];
            snprintf(msg, sizeof(msg), "%ld presses, more than %ld, in journey", got, max);
            fail(msg, nm);
        }
    } else if (strcmp(cmd, "problem") == 0 || strcmp(cmd, "expect-problem") == 0) {
        int who = res_named(argc > 1 ? argv[1] : "");
        if (who < 0) { fail("no citizen named", argc > 1 ? argv[1] : ""); return; }
        if (cmd[0] == 'p') {
            int kind = problem_kind(argc > 2 ? argv[2] : ""), it = argc > 3 ? item_named(argv[3]) : 0;
            if (kind <= PROB_NONE || kind >= PROB_COUNT || it < 0 || it >= ITEM_COUNT) fail("problem needs WHO KIND [ITEM], got", argc > 2 ? argv[2] : "");
            else if (sim_problem_raise(who, kind, 0xFFFF, it) < 0) fail("could not raise a problem for", argv[1]);
        } else if ((sim_problem_for(who) >= 0) != (argc > 2 ? atoi(argv[2]) != 0 : 1)) {
            fail(sim_problem_for(who) >= 0 ? "a problem is still open for" : "no open problem for", argv[1]);
        }
    } else if (strcmp(cmd, "item") == 0 || strcmp(cmd, "expect-item") == 0) {
        int it = item_named(argc > 1 ? argv[1] : ""), n = argc > 2 ? atoi(argv[2]) : 1;
        if (it <= ITEM_NONE || it >= ITEM_COUNT) fail("unknown item", argc > 1 ? argv[1] : "");
        else if (cmd[0] == 'i') inv_add(it, n);
        else if (inv_count(it) != n) {
            char msg[64];
            snprintf(msg, sizeof(msg), "expected %d in the bag but there are %d of", n, inv_count(it));
            fail(msg, argv[1]);
        }
    } else if (strcmp(cmd, "expect-rel") == 0) {
        int a = res_named(argc > 1 ? argv[1] : ""), b = res_named(argc > 2 ? argv[2] : "");
        int want = argc > 3 ? atoi(argv[3]) != 0 : 1;
        if (a < 0 || b < 0) fail("expect-rel needs two citizens' names, got", argc > 2 ? argv[2] : "");
        else if ((rel_kind(a, b) >= 0) != want) fail(want ? "these two have never met:" : "these two already know each other:", argv[1]);
    } else if (strcmp(cmd, "expect-money") == 0) {
        uint32_t min = (uint32_t)strtoul(argc > 1 ? argv[1] : "0", NULL, 0);
        if (doomo_town.money < min) {
            char got[24];
            snprintf(got, sizeof(got), "%u", (unsigned)doomo_town.money);
            fail(argc > 1 ? "the purse holds less than the expected H$, it has" : "", got);
        }
    /* --- end M0 --- */
    } else {
        fail("unknown command", cmd);
    }
}

static void usage(void)
{
    fprintf(stderr,
            "usage: runner [--script FILE] [--shots DIR] [--w N --h N] [--frames N]\n"
            "              [--saves DIR] [--seed N] [--zoom N] [--gba-input] [--save-stream N]\n"
            "              [--assert-no-stubs] [--check-prompts] [--pad host|gba|ps1|dc] [--quiet]\n"
            "              [--verbose] [--wav FILE] [--budgets FILE] [--frame-hash FILE]\n"
            "              [--bake-backdrops FILE]\n");
}

int main(int argc, char **argv)
{
    NatOptions o;
    const char *script = NULL, *bake = NULL;
    int frames = -1, assert_no_stubs = 0, check_prompts = 0, pad = -1, i;
    memset(&o, 0, sizeof(o));
    o.w = 240; o.h = 160; o.seed = 0x5EED1234u; o.save_dir = ".";
    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *val = i + 1 < argc ? argv[i + 1] : NULL;
        if (strcmp(a, "--script") == 0 && val) { script = val; i++; }
        else if (strcmp(a, "--shots") == 0 && val) { shots_dir = val; i++; }
        else if (strcmp(a, "--w") == 0 && val) { o.w = atoi(val); i++; }
        else if (strcmp(a, "--h") == 0 && val) { o.h = atoi(val); i++; }
        else if (strcmp(a, "--frames") == 0 && val) { frames = atoi(val); i++; }
        else if (strcmp(a, "--saves") == 0 && val) { o.save_dir = val; i++; }
        else if (strcmp(a, "--save-stream") == 0 && val) { nat_save_stream = atoi(val); i++; }
        else if (strcmp(a, "--seed") == 0 && val) { o.seed = (uint32_t)strtoul(val, NULL, 0); i++; }
        else if (strcmp(a, "--zoom") == 0 && val) { zoom = atoi(val); i++; }
        else if (strcmp(a, "--gba-input") == 0) gba_input = 1;
        else if (strcmp(a, "--assert-no-stubs") == 0) assert_no_stubs = 1;
        else if (strcmp(a, "--check-prompts") == 0) check_prompts = 1;      /* C3 */
        else if (strcmp(a, "--pad") == 0 && val) {                          /* C3: whose button names */
            static const char *const pads[PAD_COUNT] = { "host", "gba", "ps1", "dc" };
            for (pad = 0; pad < PAD_COUNT && strcmp(val, pads[pad]) != 0; pad++) {}
            if (pad == PAD_COUNT) { fprintf(stderr, "runner: unknown pad %s (host, gba, ps1, dc)\n", val); return 1; }
            i++;
        }
        else if (strcmp(a, "--quiet") == 0) o.quiet = 1;
        else if (strcmp(a, "--verbose") == 0) o.verbose = 1;
        /* --- music: musrender (MU1, SPEC sec.16.2, row 39) --- */
        else if (strcmp(a, "--wav") == 0 && val) {
            if (!nat_wav_open(val)) { fprintf(stderr, "runner: cannot write %s\n", val); return 1; }
            i++;
        }
        /* --- end music --- */
        /* --- M0: journey budgets --- */
        else if (strcmp(a, "--budgets") == 0 && val) {
            if (!load_budgets(val)) { fprintf(stderr, "runner: cannot read %s\n", val); return 1; }
            i++;
        }
        /* --- end M0 --- */
        else if (strcmp(a, "--frame-hash") == 0 && val) {   /* SP4 */
            if (!(frame_hash = fopen(val, "w"))) { fprintf(stderr, "runner: cannot write %s\n", val); return 1; }
            i++;
        }
        /* --- SP7: every backdrop at --w x --h, for the GBA ROM (stage_native.c) --- */
        else if (strcmp(a, "--bake-backdrops") == 0 && val) { bake = val; i++; }
        else if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) { usage(); return 0; }
        else { fprintf(stderr, "runner: unknown option %s\n", a); usage(); return 1; }
    }
    if (o.w > NAT_MAX_W || o.h > NAT_MAX_H) {
        fprintf(stderr, "runner: screen too big (max %dx%d)\n", NAT_MAX_W, NAT_MAX_H);
        return 1;
    }
    mkdirs(shots_dir);
    mkdirs(o.save_dir);
    nat_init(&o);
    doomo_init();
    if (pad >= 0) input_set_pad(pad);
    if (bake) {
        int n = nat_bake_backdrops(bake);
        if (!n) return 1;
        if (!o.quiet) printf("runner: %d backdrops (%dx%d) -> %s\n", n, o.w, o.h, bake);
        return 0;
    }
    if (script) {
        FILE *f = fopen(script, "r");
        char line[1024];
        if (!f) { fprintf(stderr, "runner: cannot open %s: %s\n", script, strerror(errno)); return 1; }
        script_name = script;
        while (fgets(line, sizeof(line), f)) {
            line_no++;
            line[strcspn(line, "\r\n")] = 0;
            run_line(line);
        }
        fclose(f);
    }
    if (frames > 0) step(frames);
    else if (!script) step(300);
    if (jr.on) { fprintf(stderr, "runner: journey %s never ended\n", jr.name); failures++; }   /* M0 */
    if (assert_no_stubs && stub_check("during the run")) return 2;
    /* --- C3: every prompt bar fit the screen and named only this pad's buttons --- */
    if (check_prompts) {
        UiPromptStats ps;
        ui_prompt_stats(&ps, 0);
        printf("prompts: %u bars, %u too wide, %u with a button this pad lacks\n",
               (unsigned)ps.bars, (unsigned)ps.too_wide, (unsigned)ps.wrong_pad);
        if (ps.too_wide || ps.wrong_pad) {
            fprintf(stderr, "runner: prompt bar check failed, first: \"%s\"\n", ps.first_bad);
            failures++;
        }
    }
    if (failures) {
        fprintf(stderr, "runner: %d failure(s)\n", failures);
        return 1;
    }
    return 0;
}
