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
 *   profile N           sample the PC over N instructions and print the
 *                       hottest functions (needs --nm, see below)
 * Buttons: A B L R START SELECT UP DOWN LEFT RIGHT, X (=SELECT+A),
 * Y (=SELECT+B).
 *
 * Exit status: 0 ok, 1 crash / hang / failed expectation / stub (with
 * --fail-on-stub), 2 usage or load error.
 * With --save FILE the 128 KB flash is loaded from FILE (if present) and
 * written back at exit, so persistence can be tested across runs.
 */
#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <mgba/core/config.h>
#include <mgba/core/interface.h>
#include <mgba-util/vfs.h>
#include <mgba/internal/arm/arm.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include "png.h"

#define GBA_W 240
#define GBA_H 160
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
            size_t n = strcspn(msg + 7, " \r\n");
            if (n >= sizeof(last_scene)) n = sizeof(last_scene) - 1;
            memcpy(last_scene, msg + 7, n);
            last_scene[n] = 0;
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

static uint32_t video_frames;
static uint32_t keys;

static void run_frame(void)
{
    core->setKeys(core, keys);
    core->runFrame(core);
    video_frames++;
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

/* ---- sampling profiler: "profile N" steps N instructions, sampling the
 * PC every 7th, and prints the hottest functions (needs --nm FILE with
 * the output of `arm-none-eabi-nm -n ROM.elf`). */
typedef struct Sym { uint32_t addr; char name[64]; uint32_t hits; } Sym;
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
    uint32_t i, total = 0, unknown = 0;
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
        core->step(core);
        if (i % 7 == 0) {
            uint32_t pc = (uint32_t)cpu->gprs[15];
            int lo = 0, hi = nsyms - 1, best = -1;
            if (pc_hist && pc >= focus_lo + 8 && pc < focus_hi + 8)
                pc_hist[(pc - 8 - focus_lo) / 2]++;
            while (lo <= hi) {
                int mid = (lo + hi) / 2;
                if (syms[mid].addr <= pc) { best = mid; lo = mid + 1; }
                else hi = mid - 1;
            }
            if (best >= 0 && pc - syms[best].addr < 0x10000) syms[best].hits++;
            else unknown++;
            total++;
        }
    }
    {
        Sym *copy = malloc(sizeof(Sym) * (size_t)(nsyms ? nsyms : 1));
        memcpy(copy, syms, sizeof(Sym) * (size_t)nsyms);
        qsort(copy, (size_t)nsyms, sizeof(Sym), sym_cmp_hits);
        printf("harness: profile of %u instructions (%u samples, %u unknown)\n",
               steps, total, unknown);
        for (k = 0; k < nsyms && k < 25 && copy[k].hits; k++)
            printf("harness:   %5.1f%%  %s\n", 100.0 * copy[k].hits / (total ? total : 1),
                   copy[k].name);
        free(copy);
    }
    if (pc_hist && focus_hi > focus_lo) {
        /* hottest instructions of the focus function (ARM: PC - 8) */
        uint32_t n = (focus_hi - focus_lo) / 2, j, best;
        int r;
        printf("harness: hottest addresses in %s:\n", focus);
        for (r = 0; r < 40; r++) {
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
    uint8_t *rgb = malloc((size_t)GBA_W * GBA_H * 3 * scale * scale);
    int x, y, sx, sy, w = GBA_W * scale;
    if (!rgb) return;
    for (y = 0; y < GBA_H; y++) {
        for (x = 0; x < GBA_W; x++) {
            uint32_t c = (uint32_t)video[y * GBA_W + x];
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
    if (!png_write_rgb(path, rgb, GBA_W * scale, GBA_H * scale)) {
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
static int load_save(const char *path)
{
    FILE *f = fopen(path, "rb");
    uint8_t *buf;
    long n;
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return 0; }
    buf = malloc((size_t)n);
    if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(buf); return 0; }
    fclose(f);
    if (!core->savedataRestore(core, buf, (size_t)n, false)) {
        fprintf(stderr, "harness: savedataRestore failed for %s\n", path);
        free(buf);
        return 0;
    }
    if (!quiet) printf("harness: loaded save %s (%ld bytes)\n", path, n);
    free(buf);
    return 1;
}

static void write_save(const char *path)
{
    void *data = NULL;
    size_t n = core->savedataClone(core, &data);
    FILE *f;
    if (!n || !data) {
        if (!quiet) printf("harness: no save data to write\n");
        free(data);
        return;
    }
    f = fopen(path, "wb");
    if (!f || fwrite(data, 1, n, f) != n) {
        fprintf(stderr, "harness: cannot write save %s\n", path);
        failed = 1;
    } else if (!quiet) {
        printf("harness: wrote save %s (%zu bytes)\n", path, n);
    }
    if (f) fclose(f);
    free(data);
}

int main(int argc, char **argv)
{
    const char *rom = NULL, *script = NULL, *save = NULL;
    int frames = 0, i, rc = 0;
    unsigned w = GBA_W, h = GBA_H;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--script") && i + 1 < argc) script = argv[++i];
        else if (!strcmp(argv[i], "--shots") && i + 1 < argc) shots_dir = argv[++i];
        else if (!strcmp(argv[i], "--save") && i + 1 < argc) save = argv[++i];
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--fail-on-stub")) fail_on_stub = 1;
        else if (!strcmp(argv[i], "--quiet")) quiet = 1;
        else if (!strcmp(argv[i], "--nm") && i + 1 < argc) load_nm(argv[++i]);
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
    mCoreLoadConfig(core);
    core->desiredVideoDimensions(core, &w, &h);
    if (w != GBA_W || h != GBA_H) {
        fprintf(stderr, "harness: unexpected video size %ux%u\n", w, h);
    }
    video = calloc((size_t)GBA_W * GBA_H, sizeof(color_t));
    core->setVideoBuffer(core, video, GBA_W);
    if (!mCoreLoadFile(core, rom)) { fprintf(stderr, "harness: cannot load %s\n", rom); return 2; }
    core->reset(core);
    if (save) load_save(save);

    if (script) rc = run_script(script);
    for (i = 0; i < frames && !failed; i++) run_frame();
    if (!script && !frames) run_ticks(60);

    if (save) write_save(save);
    if (failed) rc = 1;
    if (!quiet) printf("harness: done after %u video frames, %u ticks: %s\n",
                       (unsigned)video_frames, (unsigned)ticks(), rc ? "FAIL" : "OK");
    core->deinit(core);
    free(video);
    free(logbuf);
    return rc;
}
