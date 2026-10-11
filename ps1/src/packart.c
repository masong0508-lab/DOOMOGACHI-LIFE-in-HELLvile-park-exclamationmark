/* packart.c - the core's world textures and skies, kept LZ-compressed in
 * RAM (owner: ps1; the build side is ps1/tools/packart.py).
 *
 * The EXE has to fit in 2 MB together with everything else, and these
 * pixels are only read by this platform, to fill its VRAM caches. So the
 * build links them packed (packed_art.c: every frame compressed on its
 * own) and turns doomo_art_tex_pix / doomo_art_sky_pix into VIRTUAL
 * addresses (ld --defsym, CMakeLists.txt): the core's textures.c still
 * computes frame pointers from them, and the platform maps such a pointer
 * back to (frame, offset) and decodes the frame here when a cache misses.
 * Nothing may read through those pointers directly on the PS1.
 */
#include <string.h>
#include "ps1.h"

#ifdef DOOMO_PACKED_ART
extern const uint16_t ps1_pack_tex_frames, ps1_pack_sky_frames;
extern const uint32_t ps1_pack_tex_size, ps1_pack_sky_size;
extern const uint32_t ps1_pack_tex_ofs[], ps1_pack_sky_ofs[];
extern const uint8_t ps1_pack_tex[], ps1_pack_sky[];
extern const uint8_t ps1_pack_sky_edge[];

/* frame of a packed table that p points into, or -1 */
static int frame_of(const uint8_t *p, const uint8_t *base, uint32_t size, int frames,
                    uint32_t *off)
{
    uintptr_t d = (uintptr_t)p - (uintptr_t)base;
    if ((uintptr_t)p < (uintptr_t)base || d >= (uintptr_t)size * (uint32_t)frames)
        return -1;
    if (off) *off = (uint32_t)(d % size);
    return (int)(d / size);
}

/* the save codec's LZ format (packart.py) */
static void unlz(const uint8_t *src, uint8_t *dst, uint32_t n)
{
    uint8_t *d = dst, *end = dst + n;
    while (d < end) {
        uint32_t c = *src++;
        if (c < 0x80) {
            c++;
            while (c-- && d < end) *d++ = *src++;
        } else {
            uint32_t dist = (uint32_t)src[0] | ((uint32_t)src[1] << 8);
            const uint8_t *s;
            src += 2;
            c = (c & 0x7f) + 3;
            if (dist == 0 || dist > (uint32_t)(d - dst))
                return;                         /* corrupt: never happens */
            s = d - dist;
            while (c-- && d < end) *d++ = *s++;
        }
    }
}

int packart_is_tex(const uint8_t *p)
{
    return frame_of(p, &doomo_art_tex_pix[0][0], ps1_pack_tex_size, ps1_pack_tex_frames,
                    NULL) >= 0;
}

int packart_is_sky(const uint8_t *p)
{
    return frame_of(p, &doomo_art_sky_pix[0][0], ps1_pack_sky_size, ps1_pack_sky_frames,
                    NULL) >= 0;
}

/* The last texture frame decoded (vram.c uploads one frame per cache
 * miss; a texture-backed DoomoImage reads rows of the same frame). */
static uint8_t s_tex[DOOMO_TEX_SIZE_PX] __attribute__((aligned(4)));
static int s_tex_frame = -1;

const uint8_t *packart_resolve(const uint8_t *p)
{
    uint32_t off = 0;
    int f = frame_of(p, &doomo_art_tex_pix[0][0], ps1_pack_tex_size, ps1_pack_tex_frames, &off);
    if (f < 0)
        return p;                               /* not packed: the pixels */
    if (f != s_tex_frame) {
        unlz(ps1_pack_tex + ps1_pack_tex_ofs[f], s_tex, ps1_pack_tex_size);
        s_tex_frame = f;
    }
    return s_tex + off;
}

int packart_sky_decode(const uint8_t *p, uint8_t *dst)
{
    int f = frame_of(p, &doomo_art_sky_pix[0][0], ps1_pack_sky_size, ps1_pack_sky_frames, NULL);
    if (f < 0)
        return 0;
    unlz(ps1_pack_sky + ps1_pack_sky_ofs[f], dst, ps1_pack_sky_size);
    return 1;
}

int packart_sky_edges(const uint8_t *p, uint8_t *top, uint8_t *bottom)
{
    int f = frame_of(p, &doomo_art_sky_pix[0][0], ps1_pack_sky_size, ps1_pack_sky_frames, NULL);
    if (f < 0)
        return 0;
    *top = ps1_pack_sky_edge[f * 2];
    *bottom = ps1_pack_sky_edge[f * 2 + 1];
    return 1;
}

#else  /* DOOMO_PACK_ART=OFF or bring-up without the core: nothing is packed */

int packart_is_tex(const uint8_t *p) { (void)p; return 0; }
int packart_is_sky(const uint8_t *p) { (void)p; return 0; }
const uint8_t *packart_resolve(const uint8_t *p) { return p; }
int packart_sky_decode(const uint8_t *p, uint8_t *dst) { (void)p; (void)dst; return 0; }
int packart_sky_edges(const uint8_t *p, uint8_t *top, uint8_t *bottom)
{
    (void)p; (void)top; (void)bottom;
    return 0;
}

#endif
