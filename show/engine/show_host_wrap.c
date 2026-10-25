/* show_host_wrap.c - synchronous VRAM uploads for the hosts' prep (owner:
 * show, ENGINE lane 3; spec 8.2, 8.3).
 *
 * vram.c queues every upload into the frame's display list (gfx_upload_strided),
 * which the drawing shares (56 KB): a map's first frame would run out of room
 * and come out cut off. The show links with -Wl,--wrap=gfx_upload_strided;
 * while g_show_upload_sync is set (show_host.c, during a host plane's prep)
 * an upload goes to VRAM at once by LoadImage (gfx_upload_now) instead, and
 * vram.c's slot bookkeeping is exactly what it would have been. Otherwise
 * the call goes through unchanged.
 */
#include <string.h>
#include "ps1.h"
#include "show/show_hosts.h"

int __real_gfx_upload_strided(int x, int y, int w, int h, const uint8_t *src, int stride);
int __wrap_gfx_upload_strided(int x, int y, int w, int h, const uint8_t *src, int stride);

int __wrap_gfx_upload_strided(int x, int y, int w, int h, const uint8_t *src, int stride)
{
    static uint32_t buf[1024];              /* 4 KB: 64 rows of a 64-texel tile */
    int rows_per, r;
    if (!g_show_upload_sync || w <= 0 || h <= 0 || w * 2 > (int)sizeof(buf))
        return __real_gfx_upload_strided(x, y, w, h, src, stride);
    rows_per = (int)sizeof(buf) / (w * 2);
    while (h > 0) {
        int rows = h < rows_per ? h : rows_per;
        for (r = 0; r < rows; r++)
            memcpy((uint8_t *)buf + r * w * 2, src + (size_t)r * stride, (size_t)w * 2);
        gfx_upload_now(x, y, w, rows, buf);
        src += (size_t)rows * stride;
        y += rows;
        h -= rows;
    }
    return 1;
}
