#ifndef TEST_GSKIT_H
#define TEST_GSKIT_H

#include <stdint.h>

typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef struct { int Width, Height; } GSGLOBAL;
typedef struct { int Width, Height, PSM, Filter; u32 *Mem; u32 Vram; } GSTEXTURE;

enum { GS_PSM_CT16 = 2, GS_FILTER_NEAREST = 0, GSKIT_ALLOC_USERBUFFER = 1, GS_CMODE_CLAMP = 1 };
#define GSKIT_ALLOC_ERROR UINT32_MAX
#define GS_SETREG_RGBAQ(r, g, b, a, q) ((u64)(r) | (u64)(g) << 8 | (u64)(b) << 16 | (u64)(a) << 24 | (u64)(q) << 32)

u32 gsKit_vram_alloc(GSGLOBAL *gs, u32 size, int type);
u32 gsKit_texture_size(int w, int h, int psm);
void gsKit_set_clamp(GSGLOBAL *gs, int mode);
void gsKit_texture_upload(GSGLOBAL *gs, GSTEXTURE *texture);
void gsKit_clear(GSGLOBAL *gs, u64 color);
void gsKit_prim_sprite(GSGLOBAL *gs, float x0, float y0, float x1, float y1, int z, u64 color);
void gsKit_prim_sprite_texture(GSGLOBAL *gs, GSTEXTURE *texture,
                              float x0, float y0, float u0, float v0,
                              float x1, float y1, float u1, float v1, int z, u64 color);

#endif
