/* harness.c - headless libmgba test harness for the DOOMOGACHI GBA ROM
 * (owner: gba).
 *
 *   gba/build/harness ROM.gba [--script FILE] [--shots DIR] [--save FILE]
 *                     [--frames N] [--scale S] [--fail-on-stub] [--quiet]
 *                     [--nm NMFILE]   (arm-none-eabi-nm -n output, for profile)
 *
 * Runs the ROM in mGBA (no BIOS needed: HLE), feeds scripted input, prints
 * the ROM's mGBA debug log (plat_log) to stdout and writes PNG screenshots.
 *
 * Script format (same as the core native runner, one command per line,
 * '#' comments). Times are LOGIC TICKS (30 Hz doomo_frame calls): the
 * harness reads the ROM's tick counter from the mailbox at 0x02000000, so
 * scripts behave the same however fast the ROM renders.
 *   wait N              run N ticks
 *   press BTN [BTN..]   press and release (held until the ROM sampled it)
 *   hold BTN [BTN..] N  hold for N ticks, then release
 *   down BTN [BTN..]    press and keep held       (GBA harness extension)
 *   release [BTN..]     release (all if no button given)
 *   shot NAME           write SHOTS/NAME.png
 *   log TEXT            echo TEXT
 *   frames N            run N raw 60 Hz video frames  (extension)
 *   expect-log TEXT     fail unless TEXT appeared in the debug log so far
 *   expect-scene NAME   checked against "scene: NAME" log lines if the core
 *                       prints them; otherwise only a warning
 *   type "..."          not supported on GBA (warning)
 *   audio-reset         start a new audio measurement      (extension)
 *   audio [X LABEL]     print the audio peak-to-peak since audio-reset
 *   expect-audio MIN [LABEL]  fail unless peak-to-peak >= MIN (16-bit)
 *   fps [LABEL]         print logic ticks/s and drawn frames/s since the
 *                       previous "fps", and the average / worst CPU work
 *                       of a drawn tick in ms (extension)
 *   profile N           sample the PC over N instructions and print the
 *                       hottest functions (needs --nm, see below)
 *   save N / load N / expect-save N / verify N / save-must-fail N
 *                       the core's save_game / load_game / save_exists /
 *                       save_verify run inside the game ROM (mailbox
 *                       command, src/hcmd.c), like the native runner's
 *   town-fill SEED      game ROM: a worst-case free-play island (1000
 *                       random custom citizens, 2048 relationships)
 *   town-crc            game ROM: log + remember a CRC of the saved state
 *   expect-town-crc     fail unless the state's CRC equals the remembered
 *   scene NAME [ARG] / push NAME [ARG]
 *                       game ROM: scene_set / scene_push by name (mailbox)
 *   seed N              game ROM: doomo_town.seed = N, doomo_rng reseeded,
 *                       plat_entropy() pinned to N (the next town_new() is
 *                       reproducible, like the native runner's)
 *   expect-flag F [V]   fail unless story_flag(F) == V (default 1)
 *   expect-text TEXT    fail unless TEXT (<= 19 chars) is drawn by plat_text
 *                       within the next 90 ticks (stops at the first hit)
 *   story-fp [HEX]      print story_fingerprint(); with HEX, fail unless equal
 *   post CMD ARG        post mailbox command CMD (hcmd.c numbers) without
 *                       waiting, e.g. to "profile" a save while it runs
 * Buttons: A B L R START SELECT UP DOWN LEFT RIGHT, X (=SELECT+A),
 * Y (=SELECT+B).
 *
 * Exit status: 0 ok, 1 crash / hang / failed expectation / stub (with
 * --fail-on-stub), 2 usage or load error.
 * With --save FILE the 128 KB flash is backed by FILE (created if missing,
 * kept in sync while the ROM programs it, like a frontend's .sav), so
 * persistence can be tested across runs.
 */
#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <mgba/core/config.h>
#include <mgba/core/interface.h>
#include <mgba-util/vfs.h>
#include <mgba/internal/arm/arm.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/savedata.h>
#include <mgba/core/blip_buf.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include "png.h"

#define SCR_W 240
#define SCR_H 160
#define MAILBOX_ADDR 0x02000000u
#define MAILBOX_MAGIC 0x42474D44u   /* "DMGB" */

static struct mCore *core;
static color_t *video;
static int quiet;
static int fail_on_stub;
static int failed;
static int scale = 1;
static const char *shots_dir = ".";
static char *logbuf;
static size_t log_len, log_cap;
static char last_scene[64];
#define SCENE_STACK_MAX 8
static char scene_stack[SCENE_STACK_MAX][64];   /* [0] is the scene underneath all pushes */
static int scene_depth;

/* ---------------------------------------------------------------------- */
static void log_append(const char *s)
{
    size_t n = strlen(s);
    if (log_len + n + 2 > log_cap) {
        log_cap = (log_len + n + 2) * 2 + 4096;
        logbuf = realloc(logbuf, log_cap);
        if (!logbuf) { perror("realloc"); exit(2); }
    }
    memcpy(logbuf + log_len, s, n);
    log_len += n;
    logbuf[log_len++] = '\n';
    logbuf[log_len] = 0;
}

static void on_log(struct mLogger *logger, int category, enum mLogLevel level,
                   const char *format, va_list args)
{
    char msg[1024];
    const char *cat = mLogCategoryName(category);
    (void)logger;
    vsnprintf(msg, sizeof(msg), format, args);
    if (cat && strcmp(cat, "GBA Debug") == 0) {
        log_append(msg);
        if (!quiet) printf("%s\n", msg);
        if (strncmp(msg, "scene: ", 7) == 0) {
            /* "scene: set NAME" / "scene: push NAME" / "scene: pop NAME"
             * (scene.c): a small mirror of the core's scene stack, so
             * expect-scene sees the scene underneath after a pop */
            const char *p = msg + 7;
            size_t n;
            if (!strncmp(p, "set ", 4)) { p += 4; scene_depth = 0; }
            else if (!strncmp(p, "push ", 5)) p += 5;
            else if (!strncmp(p, "pop ", 4)) {
                if (scene_depth > 0) scene_depth--;
                strcpy(last_scene, scene_stack[scene_depth]);
                return;
            }
            n = strcspn(p, " \r\n");
            if (n >= sizeof(last_scene)) n = sizeof(last_scene) - 1;
            memcpy(last_scene, p, n);
            last_scene[n] = 0;
            if (scene_depth + 1 < SCENE_STACK_MAX) scene_depth++;
            strcpy(scene_stack[scene_depth], last_scene);
        }
        if (fail_on_stub && strncmp(msg, "STUB:", 5) == 0) {
            fprintf(stderr, "harness: stub ran: %s\n", msg);
            failed = 1;
        }
        if (strncmp(msg, "FATAL", 5) == 0) failed = 1;
        return;
    }
    if (level & (mLOG_FATAL | mLOG_GAME_ERROR)) {
        fprintf(stderr, "harness: mGBA %s: %s\n", cat ? cat : "?", msg);
        if (strstr(msg, "invalid") || strstr(msg, "Invalid") ||
            strstr(msg, "illegal") || strstr(msg, "Illegal") ||
            (level & mLOG_FATAL)) {
            failed = 1;
        }
    } else if ((level & (mLOG_ERROR | mLOG_WARN)) && !quiet) {
        fprintf(stderr, "harness: mGBA %s: %s\n", cat ? cat : "?", msg);
    }
}

static struct mLogger logger = { .log = on_log, .filter = NULL };

/* ---------------------------------------------------------------------- */
static uint32_t ticks(void)
{
    if (core->busRead32(core, MAILBOX_ADDR) != MAILBOX_MAGIC) return 0;
    return core->busRead32(core, MAILBOX_ADDR + 4);
}

static uint32_t drawn(void)
{
    if (core->busRead32(core, MAILBOX_ADDR) != MAILBOX_MAGIC) return 0;
    return core->busRead32(core, MAILBOX_ADDR + 8);
}

static uint32_t video_frames;
static uint32_t keys;
static void run_frame(void);

/* mailbox command (game ROM, gba/src/hcmd.c): post, run frames until the
 * ROM clears it; 0 if it never answered */
#define HC_SAVE 1
#define HC_LOAD 2
#define HC_EXISTS 3
#define HC_TOWN_FILL 4
#define HC_TOWN_CRC 5
#define HC_VERIFY 6
#define HC_SCENE 7
#define HC_PUSH 8
/* gba/src/hcmd.h (story / house runner commands, SPEC R20) */
#define HC_SEED 9
#define HC_FLAG 10
#define HC_TEXT_ARM 11
#define HC_TEXT_HIT 12
#define HC_STORY_FP 13
/* --- H2: the house renderer (gba/src/room_int.h) --- */
#define HC_ROOM_PARITY 14
#define HC_ROOM_TEST 15
static const char *golden_path;            /* --golden FILE (room-parity) */

/* "N HASH" line N of the room_ref golden; 0 if missing */
static int golden_hash(int index, uint32_t *out)
{
    FILE *f = golden_path ? fopen(golden_path, "r") : NULL;
    char line[128];
    int found = 0;
    if (!f) return 0;
    while (!found && fgets(line, sizeof(line), f)) {
        int i;
        unsigned v;
        if (line[0] != '#' && sscanf(line, "%d %x", &i, &v) == 2 && i == index) { *out = v; found = 1; }
    }
    fclose(f);
    return found;
}
/* --- end H2 --- */
/* the mailbox's 20-byte name argument (scene / push / expect-text) */
static void mailbox_name(const char *s)
{
    int i, n = (int)strlen(s);
    for (i = 0; i < 20; i++)
        core->busWrite8(core, MAILBOX_ADDR + 44 + (uint32_t)i, (uint8_t)(i < 19 && i < n ? s[i] : 0));
}

static int hcmd_silent;   /* expect-text's per-tick polls print nothing */
static int hcmd(uint32_t cmd, uint32_t arg, uint32_t *result)
{
    int n;
    if (core->busRead32(core, MAILBOX_ADDR) != MAILBOX_MAGIC) return 0;
    core->busWrite32(core, MAILBOX_ADDR + 20, arg);
    core->busWrite32(core, MAILBOX_ADDR + 16, cmd);
    for (n = 0; n < 60 * 60 && !failed; n++) {
        run_frame();
        if (core->busRead32(core, MAILBOX_ADDR + 16) == 0) {
            uint32_t cyc = core->busRead32(core, MAILBOX_ADDR + 28);
            *result = core->busRead32(core, MAILBOX_ADDR + 24);
            if (!hcmd_silent) printf("harness: command %u(%u) -> 0x%x in %.1f ms\n", (unsigned)cmd, (unsigned)arg,
                   (unsigned)*result, cyc / 16777.216);
            return 1;
        }
    }
    fprintf(stderr, "harness: command %u not answered (not the game ROM?)\n", (unsigned)cmd);
    return 0;
}

/* audio: drained every frame; peak-to-peak since "audio-reset" (the
 * output has a DC offset, silence is a flat line) */
static int aud_min = 32767, aud_max = -32768;
static uint32_t aud_samples;

static void drain_audio(void)
{
    static short buf[2 * 1024];
    blip_t *l = core->getAudioChannel(core, 0), *r = core->getAudioChannel(core, 1);
    int n, i;
    if (!l || !r) return;
    while ((n = blip_samples_avail(l)) > 0) {
        if (n > 1024) n = 1024;
        blip_read_samples(l, buf, n, 1);
        blip_read_samples(r, buf + 1, n, 1);
        for (i = 0; i < 2 * n; i++) {
            if (buf[i] < aud_min) aud_min = buf[i];
            if (buf[i] > aud_max) aud_max = buf[i];
        }
        aud_samples += (uint32_t)n;
    }
}

static void run_frame(void)
{
    core->setKeys(core, keys);
    core->runFrame(core);
    video_frames++;
    drain_audio();
}

/* run until the tick counter advanced by n (hang detection: 20 s) */
static void run_ticks(uint32_t n)
{
    uint32_t start = ticks(), guard = 0;
    while (ticks() - start < n) {
        uint32_t before = ticks();
        run_frame();
        if (ticks() == before) {
            if (++guard > (ticks() ? 1200u : 3600u)) {
                fprintf(stderr, "harness: HANG - no logic tick for 20 s (60 s at boot) "
                        "(tick %u)\n", (unsigned)ticks());
                failed = 1;
                return;
            }
        } else {
            guard = 0;
        }
        if (failed) return;
    }
}

/* ---- profiler: "profile N" steps N instructions and charges the emulated
 * cycles each one took (bus wait states, halt time) to the function its PC
 * lies in, then prints the hottest functions (needs --nm FILE with the
 * output of `arm-none-eabi-nm -n ROM.elf`). Halted time shows up in the
 * function that waits for vblank. */
typedef struct Sym { uint32_t addr; char name[64]; uint64_t hits; } Sym;
static Sym *syms;
static int nsyms;

static void load_nm(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[256];
    if (!f) { fprintf(stderr, "harness: cannot open %s\n", path); return; }
    while (fgets(line, sizeof(line), f)) {
        unsigned long a;
        char t, name[128];
        if (sscanf(line, "%lx %c %127s", &a, &t, name) != 3) continue;
        if (t != 'T' && t != 't' && t != 'W' && t != 'w') continue;
        syms = realloc(syms, sizeof(Sym) * (size_t)(nsyms + 1));
        syms[nsyms].addr = (uint32_t)a;
        name[sizeof(syms[nsyms].name) - 1] = 0;
        memcpy(syms[nsyms].name, name, sizeof(syms[nsyms].name));
        syms[nsyms].hits = 0;
        nsyms++;
    }
    fclose(f);
}

static int sym_cmp_hits(const void *a, const void *b)
{
    const Sym *x = a, *y = b;
    return x->hits < y->hits ? 1 : x->hits > y->hits ? -1 : 0;
}

static uint32_t *pc_hist;          /* per-PC samples inside `focus` */
static uint32_t focus_lo, focus_hi;

static void profile(uint32_t steps, const char *focus)
{
    struct ARMCore *cpu = core->cpu;
    struct mTiming *timing = &((struct GBA *)core->board)->timing;
    uint32_t i;
    uint64_t total = 0, unknown = 0;
    int k;
    focus_lo = focus_hi = 0;
    if (focus) {
        for (k = 0; k < nsyms; k++)
            if (!strcmp(syms[k].name, focus)) {
                focus_lo = syms[k].addr;
                focus_hi = k + 1 < nsyms ? syms[k + 1].addr : focus_lo + 0x1000;
            }
        free(pc_hist);
        pc_hist = calloc((focus_hi - focus_lo) / 2 + 1, sizeof(uint32_t));
    }
    for (k = 0; k < nsyms; k++) syms[k].hits = 0;
    core->setKeys(core, keys);
    for (i = 0; i < steps; i++) {
        /* mGBA: gprs[15] is the executing instruction + 4 (ARM) / + 2 (THUMB)
         * before the step advances the pipeline */
        uint32_t pc = (uint32_t)cpu->gprs[15] - (cpu->executionMode == MODE_THUMB ? 2 : 4);
        int32_t t0 = mTimingCurrentTime(timing), dt;
        int lo = 0, hi = nsyms - 1, best = -1;
        core->step(core);
        dt = mTimingCurrentTime(timing) - t0;
        if (dt < 0 || dt > 300000) dt = 1;       /* frame counter wrapped */
        if (pc_hist && pc >= focus_lo && pc < focus_hi)
            pc_hist[(pc - focus_lo) / 2] += (uint32_t)dt;
        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            if (syms[mid].addr <= pc) { best = mid; lo = mid + 1; }
            else hi = mid - 1;
        }
        if (best >= 0 && pc - syms[best].addr < 0x10000) syms[best].hits += (uint64_t)dt;
        else unknown += (uint64_t)dt;
        total += (uint64_t)dt;
    }
    {
        Sym *copy = malloc(sizeof(Sym) * (size_t)(nsyms ? nsyms : 1));
        memcpy(copy, syms, sizeof(Sym) * (size_t)nsyms);
        qsort(copy, (size_t)nsyms, sizeof(Sym), sym_cmp_hits);
        printf("harness: profile of %u instructions, %llu cycles (%.1f video frames), "
               "%.1f%% outside known symbols\n", steps, (unsigned long long)total,
               total / 280896.0, 100.0 * unknown / (total ? total : 1));
        for (k = 0; k < nsyms && k < 25 && copy[k].hits; k++)
            printf("harness:   %5.1f%%  %s\n", 100.0 * copy[k].hits / (total ? total : 1),
                   copy[k].name);
        free(copy);
    }
    if (pc_hist && focus_hi > focus_lo) {
        /* hottest instructions of the focus function (cycles) */
        uint32_t n = (focus_hi - focus_lo) / 2, j, best;
        int r;
        printf("harness: hottest addresses in %s:\n", focus);
        for (r = 0; r < 400; r++) {
            best = 0;
            for (j = 0; j < n; j++) if (pc_hist[j] > pc_hist[best]) best = j;
            if (!pc_hist[best]) break;
            printf("harness:   0x%08x %u\n", focus_lo + best * 2, pc_hist[best]);
            pc_hist[best] = 0;
        }
    }
}

static void shot(const char *name)
{
    char path[1024];
    uint8_t *rgb = malloc((size_t)SCR_W * SCR_H * 3 * scale * scale);
    int x, y, sx, sy, w = SCR_W * scale;
    if (!rgb) return;
    for (y = 0; y < SCR_H; y++) {
        for (x = 0; x < SCR_W; x++) {
            uint32_t c = (uint32_t)video[y * SCR_W + x];
            uint8_t r = (uint8_t)(c & 0xFF), g = (uint8_t)((c >> 8) & 0xFF),
                    b = (uint8_t)((c >> 16) & 0xFF);
            /* mGBA expands 5 -> 8 bit by shifting; restore full range */
            r |= r >> 5; g |= g >> 5; b |= b >> 5;
            for (sy = 0; sy < scale; sy++) {
                for (sx = 0; sx < scale; sx++) {
                    uint8_t *p = rgb + ((size_t)(y * scale + sy) * w + x * scale + sx) * 3;
                    p[0] = r; p[1] = g; p[2] = b;
                }
            }
        }
    }
    snprintf(path, sizeof(path), "%s/%s.png", shots_dir, name);
    if (!png_write_rgb(path, rgb, SCR_W * scale, SCR_H * scale)) {
        fprintf(stderr, "harness: cannot write %s\n", path);
        failed = 1;
    } else if (!quiet) {
        printf("harness: shot %s (tick %u)\n", path, (unsigned)ticks());
    }
    free(rgb);
}

/* ---------------------------------------------------------------------- */
static uint32_t button_bits(const char *name)
{
    static const struct { const char *n; uint32_t b; } map[] = {
        { "A", 1u << 0 }, { "B", 1u << 1 }, { "SELECT", 1u << 2 },
        { "START", 1u << 3 }, { "RIGHT", 1u << 4 }, { "LEFT", 1u << 5 },
        { "UP", 1u << 6 }, { "DOWN", 1u << 7 }, { "R", 1u << 8 },
        { "L", 1u << 9 },
        { "X", (1u << 2) | (1u << 0) }, { "Y", (1u << 2) | (1u << 1) },
    };
    size_t i;
    char up[16];
    for (i = 0; name[i] && i < sizeof(up) - 1; i++) up[i] = (char)toupper((unsigned char)name[i]);
    up[i] = 0;
    for (i = 0; i < sizeof(map) / sizeof(map[0]); i++)
        if (strcmp(up, map[i].n) == 0) return map[i].b;
    return 0;
}

static int run_script(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[1024];
    int lineno = 0;
    if (!f) { fprintf(stderr, "harness: cannot open script %s\n", path); return 2; }
    while (fgets(line, sizeof(line), f) && !failed) {
        char *argv[16];
        int argc = 0;
        char *p = line, *rest;
        lineno++;
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p || *p == '#') continue;
        p[strcspn(p, "\r\n")] = 0;
        rest = NULL;
        while (*p && argc < 16) {
            while (*p && isspace((unsigned char)*p)) p++;
            if (!*p) break;
            if (argc == 1) rest = p;
            argv[argc++] = p;
            while (*p && !isspace((unsigned char)*p)) p++;
            if (*p) *p++ = 0;
        }
        if (argc == 0) continue;
        if (!strcmp(argv[0], "wait") && argc >= 2) {
            run_ticks((uint32_t)atoi(argv[1]));
        } else if (!strcmp(argv[0], "frames") && argc >= 2) {
            int n = atoi(argv[1]);
            while (n-- > 0 && !failed) run_frame();
        } else if (!strcmp(argv[0], "press") || !strcmp(argv[0], "down") ||
                   !strcmp(argv[0], "hold")) {
            uint32_t bits = 0, n = 0;
            int i;
            for (i = 1; i < argc; i++) {
                uint32_t b = button_bits(argv[i]);
                if (b) bits |= b;
                else if (isdigit((unsigned char)argv[i][0])) n = (uint32_t)atoi(argv[i]);
                else fprintf(stderr, "harness: %s:%d unknown button %s\n", path, lineno, argv[i]);
            }
            keys |= bits;
            if (!strcmp(argv[0], "press")) {
                run_ticks(2);
                keys &= ~bits;
                run_ticks(2);
            } else if (!strcmp(argv[0], "hold")) {
                run_ticks(n ? n : 1);
                keys &= ~bits;
                run_ticks(1);
            }
        } else if (!strcmp(argv[0], "release")) {
            int i;
            if (argc == 1) keys = 0;
            for (i = 1; i < argc; i++) keys &= ~button_bits(argv[i]);
        } else if (!strcmp(argv[0], "audio-reset")) {
            aud_min = 32767; aud_max = -32768; aud_samples = 0;
        } else if (!strcmp(argv[0], "audio") || !strcmp(argv[0], "expect-audio")) {
            /* expect-audio MIN [LABEL]: fail unless the output swung by at
             * least MIN (peak to peak, 16-bit) since audio-reset */
            int p2p = aud_samples ? aud_max - aud_min : 0;
            int want = argc >= 2 ? atoi(argv[1]) : 1;
            printf("harness: audio%s%s: peak-to-peak %d over %u samples\n",
                   argc >= 3 ? " " : "", argc >= 3 ? argv[2] : "", p2p, (unsigned)aud_samples);
            if (!strcmp(argv[0], "expect-audio") && p2p < want) {
                fprintf(stderr, "harness: %s:%d expected audio (peak-to-peak >= %d), got %d\n",
                        path, lineno, want, p2p);
                failed = 1;
            }
        } else if (!strcmp(argv[0], "fps") || (!strcmp(argv[0], "expect-fps") && argc >= 2)) {
            /* logic ticks and presented frames per second since the last
             * "fps" (or the start); expect-fps MIN [LABEL] (H2) also fails
             * when fewer than MIN frames a second were drawn */
            static uint32_t t0, v0, d0;
            uint32_t t = ticks(), v = video_frames, d = drawn();
            uint32_t ws = core->busRead32(core, MAILBOX_ADDR + 32);
            uint32_t wn = core->busRead32(core, MAILBOX_ADDR + 36);
            uint32_t wm = core->busRead32(core, MAILBOX_ADDR + 40);
            static uint32_t ws0, wn0;
            int expect = argv[0][0] == 'e';
            const char *label = expect ? (argc >= 3 ? argv[2] : "") : (rest ? rest : "");
            double dfps = v > v0 ? (d - d0) * 60.0 / (v - v0) : 0.0;
            if (v > v0)
                printf("harness: fps %s%s%.1f logic ticks/s, %.1f drawn frames/s, "
                       "work avg %.1f ms max %.1f ms (%u ticks, %u video frames)\n",
                       label, label[0] ? ": " : "",
                       (t - t0) * 60.0 / (v - v0), dfps,
                       wn > wn0 ? (ws - ws0) / 16777.216 / (wn - wn0) : 0.0, wm / 16777.216,
                       (unsigned)(t - t0), (unsigned)(v - v0));
            if (expect && dfps < atof(argv[1])) {
                fprintf(stderr, "harness: %s:%d %s: %.1f drawn frames/s, expected >= %s\n",
                        path, lineno, label, dfps, argv[1]);
                failed = 1;
            }
            t0 = t; v0 = v; d0 = d;
            core->busWrite32(core, MAILBOX_ADDR + 40, 0);
            ws0 = ws; wn0 = wn;
        } else if (!strcmp(argv[0], "profile") && argc >= 2) {
            if (!nsyms) fprintf(stderr, "harness: profile needs --nm FILE\n");
            else profile((uint32_t)atoi(argv[1]), argc >= 3 ? argv[2] : NULL);
        } else if (!strcmp(argv[0], "shot") && argc >= 2) {
            shot(argv[1]);
        } else if (!strcmp(argv[0], "log")) {
            printf("harness: %s\n", rest ? rest : "");
        } else if (!strcmp(argv[0], "expect-log") && rest) {
            /* rest was split by the tokenizer: rebuild from argv */
            char want[512] = "";
            int i;
            for (i = 1; i < argc; i++) {
                if (i > 1) strncat(want, " ", sizeof(want) - strlen(want) - 1);
                strncat(want, argv[i], sizeof(want) - strlen(want) - 1);
            }
            if (!logbuf || !strstr(logbuf, want)) {
                fprintf(stderr, "harness: %s:%d expected log line containing '%s'\n",
                        path, lineno, want);
                failed = 1;
            }
        } else if (!strcmp(argv[0], "expect-scene") && argc >= 2) {
            if (!last_scene[0]) {
                fprintf(stderr, "harness: %s:%d expect-scene %s: ROM prints no "
                        "scene names (skipped)\n", path, lineno, argv[1]);
            } else if (strcmp(last_scene, argv[1]) != 0) {
                const char *s = argv[1];
                if (!strncmp(s, "SCENE_", 6)) s += 6;
                if (strcmp(last_scene, s) != 0) {
                    fprintf(stderr, "harness: %s:%d expected scene %s, got %s\n",
                            path, lineno, argv[1], last_scene);
                    failed = 1;
                }
            }
        } else if ((!strcmp(argv[0], "save") || !strcmp(argv[0], "load") ||
                    !strcmp(argv[0], "expect-save") || !strcmp(argv[0], "verify") ||
                    !strcmp(argv[0], "save-must-fail")) && argc >= 2) {
            static uint32_t r;
            uint32_t c = !strcmp(argv[0], "load") ? HC_LOAD
                       : !strcmp(argv[0], "expect-save") ? HC_EXISTS
                       : !strcmp(argv[0], "verify") ? HC_VERIFY : HC_SAVE;
            int want = strcmp(argv[0], "save-must-fail") != 0;
            if (!hcmd(c, (uint32_t)atoi(argv[1]), &r)) failed = 1;
            else if ((r != 0) != want) {
                fprintf(stderr, "harness: %s:%d %s %s %s\n", path, lineno, argv[0], argv[1],
                        want ? "failed" : "succeeded");
                failed = 1;
            }
        } else if ((!strcmp(argv[0], "scene") || !strcmp(argv[0], "push")) && argc >= 2) {
            /* game ROM: scene_set / scene_push by name, like the native runner */
            uint32_t r = 0;
            mailbox_name(argv[1]);
            if (!hcmd(argv[0][0] == 's' ? HC_SCENE : HC_PUSH,
                      argc >= 3 ? (uint32_t)atoi(argv[2]) : 0, &r) || !r) {
                fprintf(stderr, "harness: %s:%d %s %s failed\n", path, lineno, argv[0], argv[1]);
                failed = 1;
            }
        } else if (!strcmp(argv[0], "seed") && argc >= 2) {
            uint32_t r;
            if (!hcmd(HC_SEED, (uint32_t)strtoul(argv[1], NULL, 0), &r)) failed = 1;
        } else if (!strcmp(argv[0], "expect-flag") && argc >= 2) {
            uint32_t r, f = (uint32_t)strtoul(argv[1], NULL, 0);
            uint32_t want = argc >= 3 ? (uint32_t)strtoul(argv[2], NULL, 0) : 1;
            if (!hcmd(HC_FLAG, f, &r)) failed = 1;
            else if ((r != 0) != (want != 0)) {
                fprintf(stderr, "harness: %s:%d story flag %u is %u, expected %u\n", path,
                        lineno, (unsigned)f, (unsigned)r, (unsigned)want);
                failed = 1;
            }
        } else if (!strcmp(argv[0], "expect-text") && argc >= 2) {
            /* the needle is the rest of the line, rebuilt from argv */
            char want[64] = "";
            uint32_t r = 0;
            int i;
            for (i = 1; i < argc; i++) {
                if (i > 1) strncat(want, " ", sizeof(want) - strlen(want) - 1);
                strncat(want, argv[i], sizeof(want) - strlen(want) - 1);
            }
            if (strlen(want) > 19)
                fprintf(stderr, "harness: %s:%d expect-text: only the first 19 chars are "
                        "matched\n", path, lineno);
            mailbox_name(want);
            if (!hcmd(HC_TEXT_ARM, 0, &r) || !r) failed = 1;
            hcmd_silent = 1;
            for (i = 0; i < 90 && !failed; i++) {
                run_ticks(1);
                if (!hcmd(HC_TEXT_HIT, 0, &r)) failed = 1;
                else if (r) break;
            }
            hcmd_silent = 0;
            if (!failed && !r) {
                fprintf(stderr, "harness: %s:%d expected text '%s' on screen\n", path, lineno,
                        want);
                failed = 1;
            }
        } else if (!strcmp(argv[0], "story-fp")) {
            uint32_t r;
            if (!hcmd(HC_STORY_FP, 0, &r)) failed = 1;
            else {
                printf("harness: story-fp 0x%08x\n", (unsigned)r);
                if (argc >= 2 && r != (uint32_t)strtoul(argv[1], NULL, 16)) {
                    fprintf(stderr, "harness: %s:%d story fingerprint 0x%08x, expected %s\n",
                            path, lineno, (unsigned)r, argv[1]);
                    failed = 1;
                }
            }
        /* --- H2: the house renderer --- */
        } else if (!strcmp(argv[0], "room-parity") && argc >= 2) {
            /* room-parity ARG [INDEX]: the parity frame's FNV (view ARG & 3,
             * night ARG & 0x10, path ARG >> 8) against line INDEX of the
             * room_ref golden (--golden FILE) */
            uint32_t r, want;
            if (!hcmd(HC_ROOM_PARITY, (uint32_t)strtoul(argv[1], NULL, 0), &r)) failed = 1;
            else {
                printf("harness: room-parity %s: fnv %08x\n", argv[1], (unsigned)r);
                if (argc >= 3) {
                    if (!golden_hash(atoi(argv[2]), &want)) {
                        fprintf(stderr, "harness: %s:%d no golden line %s (--golden %s)\n", path,
                                lineno, argv[2], golden_path ? golden_path : "missing");
                        failed = 1;
                    } else if (r != want) {
                        fprintf(stderr, "harness: %s:%d room-parity %s: %08x, golden %08x\n",
                                path, lineno, argv[1], (unsigned)r, (unsigned)want);
                        failed = 1;
                    }
                }
            }
        } else if (!strcmp(argv[0], "room-test") && argc >= 2) {
            uint32_t r;
            if (!hcmd(HC_ROOM_TEST, (uint32_t)strtoul(argv[1], NULL, 0), &r)) failed = 1;
        /* --- end H2 --- */
        } else if (!strcmp(argv[0], "post") && argc >= 3) {
            core->busWrite32(core, MAILBOX_ADDR + 20, (uint32_t)atoi(argv[2]));
            core->busWrite32(core, MAILBOX_ADDR + 16, (uint32_t)atoi(argv[1]));
        } else if (!strcmp(argv[0], "town-fill") && argc >= 2) {
            uint32_t r;
            if (!hcmd(HC_TOWN_FILL, (uint32_t)strtoul(argv[1], NULL, 0), &r)) failed = 1;
        } else if (!strcmp(argv[0], "town-crc") || !strcmp(argv[0], "expect-town-crc")) {
            static uint32_t remembered;
            uint32_t r;
            if (!hcmd(HC_TOWN_CRC, 0, &r)) failed = 1;
            else if (!strcmp(argv[0], "town-crc")) remembered = r;
            else if (r != remembered) {
                fprintf(stderr, "harness: %s:%d town crc 0x%x, expected 0x%x\n", path, lineno,
                        (unsigned)r, (unsigned)remembered);
                failed = 1;
            }
        } else if (!strcmp(argv[0], "type")) {
            fprintf(stderr, "harness: %s:%d 'type' is not supported on GBA (skipped)\n",
                    path, lineno);
        } else {
            fprintf(stderr, "harness: %s:%d unknown command '%s'\n", path, lineno, argv[0]);
        }
    }
    fclose(f);
    return failed ? 1 : 0;
}

/* ---------------------------------------------------------------------- */
/* --save FILE: the flash is backed by FILE (created if missing, sized to
 * 128 KB by mGBA, written through a shared mapping as the game programs
 * it), exactly like a frontend's .sav file */
static int open_save(const char *path)
{
    if (!mCoreLoadSaveFile(core, path, false)) {
        fprintf(stderr, "harness: cannot open save file %s\n", path);
        return 0;
    }
    if (!quiet) printf("harness: save file %s\n", path);
    return 1;
}

static void report_save(const char *path)
{
    struct stat st;
    if (!quiet && !stat(path, &st))
        printf("harness: save file %s is %ld bytes\n", path, (long)st.st_size);
}

int main(int argc, char **argv)
{
    const char *rom = NULL, *script = NULL, *save = NULL;
    int frames = 0, i, rc = 0;
    unsigned w = SCR_W, h = SCR_H;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--script") && i + 1 < argc) script = argv[++i];
        else if (!strcmp(argv[i], "--shots") && i + 1 < argc) shots_dir = argv[++i];
        else if (!strcmp(argv[i], "--save") && i + 1 < argc) save = argv[++i];
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--fail-on-stub")) fail_on_stub = 1;
        else if (!strcmp(argv[i], "--quiet")) quiet = 1;
        else if (!strcmp(argv[i], "--nm") && i + 1 < argc) load_nm(argv[++i]);
        else if (!strcmp(argv[i], "--golden") && i + 1 < argc) golden_path = argv[++i];   /* H2 */
        else if (argv[i][0] != '-' && !rom) rom = argv[i];
        else { fprintf(stderr, "harness: bad argument %s\n", argv[i]); return 2; }
    }
    if (scale < 1) scale = 1;
    if (scale > 4) scale = 4;
    if (!rom) {
        fprintf(stderr, "usage: harness ROM.gba [--script F] [--shots DIR] "
                "[--save F] [--frames N] [--scale S] [--fail-on-stub] [--quiet]\n");
        return 2;
    }
    mkdir(shots_dir, 0755);
    mLogSetDefaultLogger(&logger);
    core = mCoreFind(rom);
    if (!core || !core->init(core)) { fprintf(stderr, "harness: no core for %s\n", rom); return 2; }
    mCoreInitConfig(core, NULL);
    mCoreConfigSetDefaultValue(&core->config, "idleOptimization", "ignore");
    mCoreConfigSetDefaultIntValue(&core->config, "skipBios", 1);
    mCoreConfigSetDefaultIntValue(&core->config, "volume", 0x100);   /* else muted */
    mCoreConfigSetDefaultIntValue(&core->config, "mute", 0);
    mCoreLoadConfig(core);
    core->desiredVideoDimensions(core, &w, &h);
    if (w != SCR_W || h != SCR_H) {
        fprintf(stderr, "harness: unexpected video size %ux%u\n", w, h);
    }
    video = calloc((size_t)SCR_W * SCR_H, sizeof(color_t));
    core->setVideoBuffer(core, video, SCR_W);
    if (!mCoreLoadFile(core, rom)) { fprintf(stderr, "harness: cannot load %s\n", rom); return 2; }
    if (save && !open_save(save)) return 2;
    core->reset(core);
    /* the cartridge has a 128 KB FLASH (FLASH1M_V103): tell mGBA up front
     * instead of letting it guess on first use, so the save can be
     * restored / exported (--save) */
    if (core->platform(core) == mPLATFORM_GBA)
        GBASavedataForceType(&((struct GBA *)core->board)->memory.savedata, SAVEDATA_FLASH1M);

    if (script) rc = run_script(script);
    for (i = 0; i < frames && !failed; i++) run_frame();
    if (!script && !frames) run_ticks(60);

    if (failed) rc = 1;
    if (!quiet) printf("harness: done after %u video frames, %u ticks: %s\n",
                       (unsigned)video_frames, (unsigned)ticks(), rc ? "FAIL" : "OK");
    core->deinit(core);
    if (save) report_save(save);
    free(video);
    free(logbuf);
    return rc;
}
