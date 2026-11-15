/* talkdisc.c - Talk's database from the disc (owner: dreamcast; TD1, docs/TALK.md, plat_ext.h).
 *
 * TALK_<code>.BIN (core/tools/talkc.py, "make -C core talk") sits on the disc root, one file per
 * language, named in capitals like every ISO 9660 name (TALK_EN.BIN). plat_talk_load() reads the
 * whole file through KallistiOS's CD file system into the heap, one block, and plat_talk_read() is a
 * bounds-checked memcpy out of it: Talk reads a row or a string at a time, so a disc read per call
 * would stall the frame. The English bank is about 0.8 MB; the Dreamcast has 16 MB and the heap is
 * 5.9 MB above the image. Loading another language frees the first. A file that is missing or
 * cannot be read leaves nothing loaded: plat_talk_load() returns 0, Talk says "not here".
 * plat_features() (pad.c) reports PLATF_TALK on this build.
 */
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <kos/fs.h>
#include "dc_plat.h"
#include "doomo/plat_ext.h"

#define TALK_NAME_MAX 24

static uint8_t *s_bank;                   /* the loaded file (malloc), NULL when none */
static uint32_t s_size;                   /* its size in bytes */
static char     s_code[8];                /* the language code it belongs to ("en") */

static void talk_unload(void)
{
    free(s_bank);
    s_bank = NULL;
    s_size = 0;
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

uint32_t plat_talk_load(const char *code)
{
    char path[TALK_NAME_MAX];
    file_t fd;
    ssize_t total;
    uint8_t *buf;
    if (s_bank && code && strcmp(code, s_code) == 0) return s_size;   /* already here */
    talk_unload();
    if (!talk_path(path, code)) return 0;
    fd = fs_open(path, O_RDONLY);
    if (fd < 0) return 0;
    total = fs_total(fd);
    if (total <= 0) { fs_close(fd); return 0; }
    buf = (uint8_t *)malloc((size_t)total);
    if (!buf) { fs_close(fd); return 0; }
    if (fs_read(fd, buf, (size_t)total) != total) { fs_close(fd); free(buf); return 0; }
    fs_close(fd);
    s_bank = buf;
    s_size = (uint32_t)total;
    strcpy(s_code, code);
    return s_size;
}

uint32_t plat_talk_read(uint32_t off, void *dst, uint32_t len)
{
    if (!s_bank || off > s_size || len > s_size - off) return 0;
    memcpy(dst, s_bank + off, len);
    return len;
}
