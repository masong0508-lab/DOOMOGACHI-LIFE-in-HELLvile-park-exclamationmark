/* mock of the KallistiOS file calls packdisc.c uses, over the host's stdio (dc/tests/packdisc_host.sh) */
#ifndef MOCK_KOS_FS_H
#define MOCK_KOS_FS_H
#include <sys/types.h>
typedef int file_t;
file_t  fs_open(const char *fn, int mode);
int     fs_close(file_t fd);
ssize_t fs_read(file_t fd, void *buf, size_t n);
off_t   fs_seek(file_t fd, off_t off, int whence);
ssize_t fs_total(file_t fd);
#endif
