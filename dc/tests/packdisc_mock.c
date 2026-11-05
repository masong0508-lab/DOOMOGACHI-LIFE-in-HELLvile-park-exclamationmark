#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "kos/fs.h"
long mock_reads;                       /* fs_read calls, so the test can see the window working */
static int fd = -1;
file_t fs_open(const char *fn, int mode)
{
    (void)mode;
    if (strcmp(fn, "/cd/PACK.DMP")) return -1;
    fd = open(getenv("PACK_FILE"), O_RDONLY);
    return fd;
}
int fs_close(file_t f) { return close(f); }
ssize_t fs_read(file_t f, void *b, size_t n) { mock_reads++; return read(f, b, n); }
off_t fs_seek(file_t f, off_t o, int w) { return lseek(f, o, w); }
ssize_t fs_total(file_t f) { return lseek(f, 0, SEEK_END) < 0 ? -1 : (lseek(f, 0, SEEK_CUR)); }
