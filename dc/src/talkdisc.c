/* talkdisc.c - Talk's database from the disc (owner: dreamcast; TD1, docs/TALK.md, plat_ext.h).
 *
 * TALK_<code>.BIN (core/tools/talkc.py, "make -C core talk") sits on the disc root, one file per
 * language, named in capitals like every ISO 9660 name (TALK_EN.BIN). The whole file goes into the heap
 * (the English bank is about 0.9 MB; the Dreamcast has 16 MB and the heap is 5.8 MB above the image) and
 * plat_talk_read() is a bounds-checked memcpy out of it: Talk reads a row or a string at a time, so a
 * disc read per call would stall the frame.
 * New discs also carry TALK_<code>.LZB: a lossless sector-aligned TLZ1
 * stream (tools/talkpack.py), decoded directly into the same heap bank.
 * Only one sector of temporary storage is needed; raw BIN discs still work.
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
#define TALK_SECTOR 2048u
#define TALK_DECODE_MAX 8192u
#define TALK_BANK_MAX (8u * 1024u * 1024u)
#ifndef TALK_CHUNK
#define TALK_CHUNK 2048u                  /* bytes per slice: one CD sector (Flycast takes about 17 ms for it) */
#endif
#ifndef TALK_SLICE_GUESS
#define TALK_SLICE_GUESS 20000u           /* us a slice is assumed to take until one has run (Flycast: 19 ms; a busy tick passes 0) */
#endif
#ifndef TALK_PREFETCH_TICKS
#define TALK_PREFETCH_TICKS 90u           /* ticks after boot (3 s) before the pump starts the English bank */
#endif
#ifndef TALK_SLICE_MIN_US
#define TALK_SLICE_MIN_US 5000u           /* a cached/emulated zero-time read must not exhaust the whole bank in one tick */
#endif

static uint8_t *s_bank;                   /* the file (malloc): whole once s_fd < 0, else s_got bytes are in */
static uint32_t s_size;                   /* its size in bytes (0: nothing started) */
static uint32_t s_got;                    /* decoded bytes so far */
static uint32_t s_sectors;                /* unread packed sectors, including the header */
static int      s_packed;
static uint8_t  s_sector[TALK_SECTOR];
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
    s_size = s_got = s_sectors = 0;
    s_packed = 0;
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
    /* Prefer the packed copy; retain the old raw path for legacy discs. */
    memcpy(path + strlen(path) - 3, "LZB", 3);
    s_fd = fs_open(path, O_RDONLY);
    if (s_fd >= 0) {
        s_packed = 1;
        total = fs_total(s_fd);
        if (total < (ssize_t)(2u * TALK_SECTOR) || total % TALK_SECTOR != 0) {
            talk_unload();
            return 0;
        }
        s_sectors = (uint32_t)total / TALK_SECTOR;
        /* The first idle slice reads the header and allocates the output bank. */
    } else {
        memcpy(path + strlen(path) - 3, "BIN", 3);
        s_fd = fs_open(path, O_RDONLY);
        if (s_fd < 0) return 0;
        total = fs_total(s_fd);
        s_bank = total > 0 ? (uint8_t *)malloc((size_t)total) : NULL;
        if (!s_bank) { talk_unload(); return 0; }
        s_size = (uint32_t)total;
    }
    strcpy(s_code, code);
    return 1;
}

/* Decode one independent sector's token framing. Match history lives
 * in the bank itself and may span sectors. All reads/writes are bounded
 * before copying; malformed streams never become visible to the core. */
static int talk_unpack_sector(void)
{
    uint32_t end = (uint32_t)s_sector[0] | ((uint32_t)s_sector[1] << 8);
    uint32_t at = 2, start = s_got;
    if (!end || end > TALK_SECTOR - 2u) return 0;
    end += 2;
    while (at < end) {
        uint32_t tag = s_sector[at++], n, dist;
        n = tag < 128u ? tag + 1u : (tag & 63u) + 3u;
        if (n > s_size - s_got || n > TALK_DECODE_MAX - (s_got - start)) return 0;
        if (tag < 128u) {
            if (n > end - at) return 0;
            memcpy(s_bank + s_got, s_sector + at, n);
            at += n;
            s_got += n;
        } else {
            if (tag < 192u) {
                if (at == end) return 0;
                dist = (uint32_t)s_sector[at++] + 1u;
            } else {
                if (end - at < 2u) return 0;
                dist = (uint32_t)s_sector[at] | ((uint32_t)s_sector[at + 1] << 8);
                at += 2;
            }
            if (!dist || dist > s_got) return 0;
            /* Forward byte copy deliberately supports overlapping LZ matches. */
            while (n--) {
                s_bank[s_got] = s_bank[s_got - dist];
                s_got++;
            }
        }
    }
    return 1;
}

static void talk_packed_step(void)
{
    if (!s_sectors || fs_read(s_fd, s_sector, TALK_SECTOR) != (ssize_t)TALK_SECTOR) {
        talk_unload();
        return;
    }
    s_sectors--;
    if (!s_size) {
        uint32_t size = (uint32_t)s_sector[4] | ((uint32_t)s_sector[5] << 8) |
                        ((uint32_t)s_sector[6] << 16) | ((uint32_t)s_sector[7] << 24);
        if (memcmp(s_sector, "TLZ1", 4) || size < 20u || size > TALK_BANK_MAX || !s_sectors) {
            talk_unload();
            return;
        }
        s_bank = (uint8_t *)malloc(size);
        if (!s_bank) { talk_unload(); return; }
        s_size = size;
    } else if (!talk_unpack_sector()) {
        talk_unload();
        return;
    }
    if (!s_sectors || s_got == s_size) {
        if (s_sectors || s_got != s_size || memcmp(s_bank, "TALK", 4)) {
            talk_unload();
            return;
        }
        fs_close(s_fd);
        s_fd = -1;
    }
}

/* one slice; the last one closes the file */
static void talk_step(void)
{
    uint32_t n;
    if (s_packed) { talk_packed_step(); return; }
    n = s_size - s_got;
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
        /* The SH4 timer may report a near-zero slice from a cache hit or
         * a Flycast host clock. That is NOT evidence that the next GD-ROM
         * seek is free: reserve a 5ms minimum against the current tick's
         * slot, even when the measured read is instant. This also bounds
         * how much of the bank can be pumped in one idle interval. */
        if (used < TALK_SLICE_MIN_US) used = TALK_SLICE_MIN_US;
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
    if (code && (s_size || s_fd >= 0) && strcmp(code, s_code) == 0)     /* the one we have, or are reading */
        return s_fd >= 0 ? PLAT_TALK_LOADING : s_size;
    return talk_begin(code) ? PLAT_TALK_LOADING : 0;
}

uint32_t plat_talk_read(uint32_t off, void *dst, uint32_t len)
{
    if (!s_bank || s_fd >= 0 || off > s_size || len > s_size - off) return 0;
    memcpy(dst, s_bank + off, len);
    return len;
}
