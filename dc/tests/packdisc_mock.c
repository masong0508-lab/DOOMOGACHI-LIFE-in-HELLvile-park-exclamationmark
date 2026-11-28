#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "kos/fs.h"
long mock_opens;                       /* fs_open calls that found a file */
long mock_maxread;                     /* the biggest single fs_read, bytes */
long mock_reads;                       /* fs_read calls, so the test can see the window working */
static int fd = -1;
file_t fs_open(const char *fn, int mode)
{
    (void)mode;
    if (!strcmp(fn, "/cd/PACK.DMP") && getenv("PACK_FILE")) { fd = open(getenv("PACK_FILE"), O_RDONLY); return fd; }
    /* TD1: the Talk bank, and only under the capitals the disc has (ISO 9660 names) */
    if (!strcmp(fn, "/cd/TALK_EN.LZB") && getenv("TALK_PACKED_FILE")) { mock_opens++; fd = open(getenv("TALK_PACKED_FILE"), O_RDONLY); return fd; }
    if (!strcmp(fn, "/cd/TALK_EN.BIN") && getenv("TALK_FILE")) { mock_opens++; fd = open(getenv("TALK_FILE"), O_RDONLY); return fd; }
    return -1;
}
int fs_close(file_t f) { return close(f); }
ssize_t fs_read(file_t f, void *b, size_t n) { mock_reads++; if ((long)n > mock_maxread) mock_maxread = (long)n; return read(f, b, n); }
off_t fs_seek(file_t f, off_t o, int w) { return lseek(f, o, w); }
/* like KOS's fs_total: the size, the file position untouched */
ssize_t fs_total(file_t f) { off_t at = lseek(f, 0, SEEK_CUR), n = lseek(f, 0, SEEK_END); lseek(f, at, SEEK_SET); return n; }
