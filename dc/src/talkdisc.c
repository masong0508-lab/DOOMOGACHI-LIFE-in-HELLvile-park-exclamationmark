/* talkdisc.c - Talk's database from the disc (owner: dreamcast; TD1, docs/TALK.md, plat_ext.h).
 *
 * TALK_<code>.BIN (core/tools/talkc.py, "make -C core talk") sits on the disc root, one file per
 * language, named in capitals like every ISO 9660 name (TALK_EN.BIN). The whole file goes into the heap
 * (the English bank is about 0.9 MB; the Dreamcast has 16 MB and the heap is 5.8 MB above the image) and
 * plat_talk_read() is a bounds-checked memcpy out of it: Talk reads a row or a string at a time, so a
 * disc read per call would stall the frame.
 *
 * The disc is slow (Flycast: one read of the whole bank held one tick for 7.4 s; one sector takes about
 * 17 ms), so the bank is read in TALK_CHUNK slices by dc_talk_pump() (main.c, in the idle time at the end
 * of a tick): as many slices as the slot has time for, and none when the last one would not fit, so a tick
 * never pays for the file (the first slice also waits for a slot of TALK_SLICE_GUESS; the rule rests on the
 * last slice's time, measured on Flycast only: a real GD-ROM seek after PACK.DMP or the FMV may take longer). The pump starts the English bank by itself once the game has run
 * TALK_PREFETCH_TICKS ticks, so it is there by the time a player reaches a housemate; an early
 * plat_talk_load() starts the load too and answers PLAT_TALK_LOADING until the last slice is in (Talk is
 * "not ready" meanwhile: the A press does nothing and works a moment later). Loading another language
 * frees the first. A file that is missing or cannot be read leaves nothing loaded: plat_talk_load()
 * returns 0 (Talk then says "not here"). plat_features() (pad.c) reports PLATF_TALK on this build.
 */
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <kos/fs.h>
#include "dc_plat.h"
#include "doomo/plat_ext.h"

#define TALK_NAME_MAX 24
#ifndef TALK_CHUNK
#define TALK_CHUNK 2048u                  /* bytes per slice: one CD sector (Flycast takes about 17 ms for it) */
#endif
#ifndef TALK_SLICE_GUESS
#define TALK_SLICE_GUESS 20000u           /* us a slice is assumed to take until one has run (Flycast: 19 ms; a busy tick passes 0) */
#endif
#ifndef TALK_PREFETCH_TICKS
#define TALK_PREFETCH_TICKS 90u           /* ticks after boot (3 s) before the pump starts the English bank */
#endif

static uint8_t *s_bank;                   /* the file (malloc): whole once s_fd < 0, else s_got bytes are in */
static uint32_t s_size;                   /* its size in bytes (0: nothing started) */
static uint32_t s_got;                    /* bytes read so far */
static file_t   s_fd = -1;                /* the open file while slices are still to come */
static char     s_code[8];                /* the language code it belongs to ("en") */
static uint32_t s_idle;                   /* pump calls before a load was started */
static uint32_t s_slice_us = TALK_SLICE_GUESS;   /* what the last slice took (a guess before the first) */
static uint32_t s_slice_max;              /* the longest slice, for the log line when the file is in */
static int      s_tried;                  /* the pump started (or failed to start) the prefetch: not again */

static void talk_unload(void)
{
    if (s_fd >= 0) fs_close(s_fd);
    s_fd = -1;
    free(s_bank);
    s_bank = NULL;
    s_size = s_got = 0;
    s_code[0] = 0;
}

/* "/cd/TALK_" + the code in capitals + ".BIN"; 0 when the code is empty or too long */
static int talk_path(char *out, const char *code)
{
    size_t n = code ? strlen(code) : 0, i;
    if (n == 0 || n >= sizeof s_code) return 0;
    memcpy(out, "/cd/TALK_", 9);
    for (i = 0; i < n; i++) {
        char c = code[i];
        out[9 + i] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
    }
    memcpy(out + 9 + n, ".BIN", 5);
    return 1;
}

/* open the file and size the block; the slices come from dc_talk_pump(). 0 on failure (nothing loaded). */
static int talk_begin(const char *code)
{
    char path[TALK_NAME_MAX];
    ssize_t total;
    talk_unload();
    if (!talk_path(path, code)) return 0;
    s_fd = fs_open(path, O_RDONLY);
    if (s_fd < 0) return 0;
    total = fs_total(s_fd);
    s_bank = total > 0 ? (uint8_t *)malloc((size_t)total) : NULL;
    if (!s_bank) { talk_unload(); return 0; }
    s_size = (uint32_t)total;
    strcpy(s_code, code);
    return 1;
}

/* one slice; the last one closes the file */
static void talk_step(void)
{
    uint32_t n = s_size - s_got;
    if (n > TALK_CHUNK) n = TALK_CHUNK;
    if (fs_read(s_fd, s_bank + s_got, n) != (ssize_t)n) { talk_unload(); return; }
    s_got += n;
    if (s_got == s_size) { fs_close(s_fd); s_fd = -1; }
}

/* us_left: the microseconds of this tick's slot still free (main.c passes the idle time before the next tick);
 * a slice runs only when the last one fit in it, so a busy tick never pays for the disc. */
void dc_talk_pump(uint32_t us_left)
{
    while (s_fd >= 0 && us_left >= s_slice_us) {          /* a fast disc takes several slices in a slot */
        uint32_t t0 = dc_us(), used;
        talk_step();
        used = dc_us() - t0;
        s_slice_us = used;
        if (used > s_slice_max) s_slice_max = used;
        us_left = used < us_left ? us_left - used : 0;
        if (s_fd < 0 && s_bank)
            dc_logf("talkdisc: TALK_%s.BIN read, %u bytes, slice max %u us", s_code, (unsigned)s_size, (unsigned)s_slice_max);
    }
    if (s_fd >= 0) return;
    if (!s_bank && !s_tried && ++s_idle >= TALK_PREFETCH_TICKS) {
        s_tried = 1;
        talk_begin("en");
    }
}

uint32_t plat_talk_load(const char *code)
{
    if (code && s_size && strcmp(code, s_code) == 0)     /* the one we have, or are reading */
        return s_fd >= 0 ? PLAT_TALK_LOADING : s_size;
    return talk_begin(code) ? PLAT_TALK_LOADING : 0;
}

uint32_t plat_talk_read(uint32_t off, void *dst, uint32_t len)
{
    if (!s_bank || s_fd >= 0 || off > s_size || len > s_size - off) return 0;
    memcpy(dst, s_bank + off, len);
    return len;
}
