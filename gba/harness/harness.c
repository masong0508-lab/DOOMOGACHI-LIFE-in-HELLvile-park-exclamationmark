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
 *                       previous "fps" (extension)
 *   profile N           sample the PC over N instructions and print the
 *                       hottest functions (needs --nm, see below)
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

static uint32_t drawn(void)
{
    if (core->busRead32(core, MAILBOX_ADDR) != MAILBOX_MAGIC) return 0;
    return core->busRead32(core, MAILBOX_ADDR + 8);
}

static uint32_t video_frames;
static uint32_t keys;

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
        } else if (!strcmp(argv[0], "fps")) {
            /* logic ticks and presented frames per second since the last
             * "fps" (or the start) */
            static uint32_t t0, v0, d0;
            uint32_t t = ticks(), v = video_frames, d = drawn();
            if (v > v0)
                printf("harness: fps %s%s%.1f logic ticks/s, %.1f drawn frames/s "
                       "(%u ticks, %u video frames)\n", rest ? rest : "", rest ? ": " : "",
                       (t - t0) * 60.0 / (v - v0), (d - d0) * 60.0 / (v - v0),
                       (unsigned)(t - t0), (unsigned)(v - v0));
            t0 = t; v0 = v; d0 = d;
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
