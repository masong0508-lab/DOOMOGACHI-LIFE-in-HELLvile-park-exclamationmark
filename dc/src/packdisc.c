/* packdisc.c - the mature content pack, read from the disc (owner: dreamcast; E10).
 *
 * An optional PACK.DMP (a DMPK v1 pack built for port dc by tools/mpack/mpack.py, E7) sits on the
 * disc root. Nothing of it is resident: core/src/pack.c opens it through plat_pack_read(), which
 * reads through KallistiOS's CD file system (/cd/PACK.DMP) with a 32 KB window (the open pass reads
 * the header, the directory and the CRC in small sequential steps; the window turns those into one
 * disc read per 32 KB), and reads a request of a window or more straight into the caller's buffer.
 * The mslot scene reads the slot's script whole into plat_mslot_script_buf() (PACK_SCRIPT_CAP_DC)
 * and decodes a still into plat_mslot_buf() (PACK_CG_W x PACK_CG_H bytes).
 *
 * Built only when the disc has a pack (make -C dc PACK=/path/pack.dmpk, which also defines
 * DOOMO_PACK_ON_DISC): the stock disc links none of this. With a pack: 32 KB window + 32 KB script
 * buffer + 75 KB still buffer, all BSS (the Dreamcast has 16 MB). plat_image is still a stub here
 * (DC3), so the still is decoded and handed over but only drawn once the 2D layer lands.
 */
#if defined(DOOMO_PACK_ON_DISC)
#include <fcntl.h>
#include <string.h>
#include <kos/fs.h>
#include "dc_plat.h"
#include "doomo/pack.h"
#include "doomo/plat_ext.h"

#define PACK_PATH "/cd/PACK.DMP"
#define WIN       (32u * 1024u)

static uint8_t  s_win[WIN] __attribute__((aligned(32)));
static uint8_t  s_script[PACK_SCRIPT_CAP_DC] __attribute__((aligned(32)));
static uint8_t  s_still[PACK_CG_W * PACK_CG_H] __attribute__((aligned(32)));
static file_t   s_fd = -2;                /* -2 not tried yet, -1 missing, else the open file */
static uint32_t s_size;                   /* file size in bytes */
static uint32_t s_wbase;                  /* file offset of s_win[0] */
static uint32_t s_wlen;                   /* valid bytes in s_win (0: none) */

static int pack_file(void)
{
    if (s_fd == -2) {
        s_fd = fs_open(PACK_PATH, O_RDONLY);
        if (s_fd >= 0) {
            ssize_t t = fs_total(s_fd);
            if (t <= 0) { fs_close(s_fd); s_fd = -1; }
            else s_size = (uint32_t)t;
        } else {
            s_fd = -1;
        }
    }
    return s_fd >= 0;
}

static int read_at(uint32_t off, void *dst, uint32_t n)
{
    return fs_seek(s_fd, (off_t)off, SEEK_SET) == (off_t)off && fs_read(s_fd, dst, n) == (ssize_t)n;
}

uint32_t plat_pack_read(uint32_t off, void *dst, uint32_t len)
{
    uint8_t *d = (uint8_t *)dst;
    uint32_t done = 0;
    if (!pack_file() || off > s_size || len > s_size - off)
        return 0;
    while (done < len) {
        uint32_t pos = off + done, left = len - done, n;
        if (s_wlen && pos >= s_wbase && pos < s_wbase + s_wlen) {
            n = s_wbase + s_wlen - pos;
            if (n > left) n = left;
            memcpy(d + done, s_win + (pos - s_wbase), n);
        } else if (left >= WIN) {                         /* big request: straight into the caller's buffer */
            n = left;
            if (!read_at(pos, d + done, n)) return 0;
        } else {                                          /* small: refill the window around pos */
            uint32_t base = pos & ~(WIN - 1u);
            n = s_size - base < WIN ? s_size - base : WIN;
            s_wlen = 0;
            if (!read_at(base, s_win, n)) return 0;
            s_wbase = base;
            s_wlen = n;
            continue;
        }
        done += n;
    }
    return done;
}

uint8_t *plat_mslot_script_buf(uint32_t *cap)
{
    if (cap) *cap = sizeof s_script;
    return s_script;
}

uint8_t *plat_mslot_buf(uint32_t *cap)
{
    if (cap) *cap = sizeof s_still;
    return s_still;
}
#endif
