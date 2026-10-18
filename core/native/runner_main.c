/* runner_main.c - the headless runner: boots the core, plays a scripted
 * input file and writes PNG screenshots (owner: foundation).
 *
 *   runner [--script FILE] [--shots DIR] [--w 240 --h 160] [--frames N]
 *          [--saves DIR] [--seed N] [--zoom N] [--gba-input] [--save-stream N]
 *          [--assert-no-stubs] [--quiet] [--verbose] [--wav FILE]
 * --wav FILE records the game's audio (22,050 Hz stereo, musrender: the
 * music SPEC's listening renders, core/native/plat_music_native.c).
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
 *   log TEXT            print TEXT
 *   no-stubs            fail now if any stub has run
 *   seed N              (S2, SPEC C18) doomo_town.seed = N, and N is what
 *                       plat_entropy() returns from now (the next new town)
 *   expect-text "TEXT"  fail unless the dialogue box's line contains TEXT
 *   expect-flag F [V]   fail unless story flag F (0..255) is V (default 1)
 *   story-fp [HEX]      print story_fingerprint(); with HEX, fail unless equal
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

static uint32_t held;
static int gba_input;
static const char *shots_dir = ".";
static int zoom = 1;
static int failures;
static const char *script_name = "(no script)";
static int line_no;

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

static void step(int n)
{
    while (n-- > 0) {
        plat_begin_frame();
        doomo_frame(held | (gba_input ? DOOMO_INPUT_SYNTH_XY : 0));
        plat_end_frame();
    }
}

static uint32_t button(const char *s)
{
    static const struct { const char *n; uint32_t b; } names[] = {
        { "UP", BTN_UP }, { "DOWN", BTN_DOWN }, { "LEFT", BTN_LEFT },
        { "RIGHT", BTN_RIGHT }, { "A", BTN_A }, { "B", BTN_B }, { "L", BTN_L },
        { "R", BTN_R }, { "START", BTN_START }, { "SELECT", BTN_SELECT },
        { "X", BTN_X }, { "Y", BTN_Y }, { "L2", BTN_L2 }, { "R2", BTN_R2 }
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
    if (strcmp(cmd, "wait") == 0) {
        step(argc > 1 ? atoi(argv[1]) : 1);
    } else if (strcmp(cmd, "press") == 0) {
        uint32_t b = 0;
        for (i = 1; i < argc; i++) {
            uint32_t one = button(argv[i]);
            if (!one) fail("unknown button", argv[i]);
            b |= one;
        }
        held |= b;
        step(1);
        held &= ~b;
        step(1);
    } else if (strcmp(cmd, "hold") == 0) {
        uint32_t b = argc > 1 ? button(argv[1]) : 0;
        if (!b) { fail("unknown button", argc > 1 ? argv[1] : ""); return; }
        held |= b;
        step(argc > 2 ? atoi(argv[2]) : 1);
        held &= ~b;
    } else if (strcmp(cmd, "down") == 0) {
        uint32_t b = argc > 1 ? button(argv[1]) : 0;
        if (!b) fail("unknown button", argc > 1 ? argv[1] : "");
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
    } else if (strcmp(cmd, "story-fp") == 0) {
        uint32_t fp = story_fingerprint();
        printf("story-fp: 0x%08x\n", (unsigned)fp);
        if (argc > 1 && (uint32_t)strtoul(argv[1], NULL, 16) != fp) fail("story fingerprint differs from", argv[1]);
    /* --- end S2 --- */
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
    } else {
        fail("unknown command", cmd);
    }
}

static void usage(void)
{
    fprintf(stderr,
            "usage: runner [--script FILE] [--shots DIR] [--w N --h N] [--frames N]\n"
            "              [--saves DIR] [--seed N] [--zoom N] [--gba-input] [--save-stream N]\n"
            "              [--assert-no-stubs] [--quiet] [--verbose] [--wav FILE]\n");
}

int main(int argc, char **argv)
{
    NatOptions o;
    const char *script = NULL;
    int frames = -1, assert_no_stubs = 0, i;
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
        else if (strcmp(a, "--quiet") == 0) o.quiet = 1;
        else if (strcmp(a, "--verbose") == 0) o.verbose = 1;
        /* --- music: musrender (MU1, SPEC sec.16.2, row 39) --- */
        else if (strcmp(a, "--wav") == 0 && val) {
            if (!nat_wav_open(val)) { fprintf(stderr, "runner: cannot write %s\n", val); return 1; }
            i++;
        }
        /* --- end music --- */
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
    if (assert_no_stubs && stub_check("during the run")) return 2;
    if (failures) {
        fprintf(stderr, "runner: %d failure(s)\n", failures);
        return 1;
    }
    return 0;
}
