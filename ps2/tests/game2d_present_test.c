/* Run the real PS2 presenter against a recording GS command sink. */
#include <stdio.h>
#include "game2d.h"
#include "doomo/platform.h"

const uint16_t doomo_palette[256] = { [11] = 0x221F };

static int failures, uploads, clears, fills, pictures;
static uint16_t uploaded;
static struct { int x0, y0, x1, y1; u64 color; } rects[2], picture;

#define CHECK(p) do { if (!(p)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #p); failures++; } } while (0)

u32 gsKit_vram_alloc(GSGLOBAL *gs, u32 size, int type)
{
    (void)gs;
    CHECK(size == 153600 && type == GSKIT_ALLOC_USERBUFFER);
    return 8192;
}

u32 gsKit_texture_size(int w, int h, int psm)
{
    CHECK(psm == GS_PSM_CT16);
    return (u32)(w * h * 2);
}

void gsKit_set_clamp(GSGLOBAL *gs, int mode)
{
    (void)gs;
    CHECK(mode == GS_CMODE_CLAMP);
}

void gsKit_texture_upload(GSGLOBAL *gs, GSTEXTURE *texture)
{
    (void)gs;
    uploads++;
    uploaded = ((const u16 *)(const void *)texture->Mem)[0];
}

void gsKit_clear(GSGLOBAL *gs, u64 color)
{
    (void)gs;
    (void)color;
    clears++;
}

void gsKit_prim_sprite(GSGLOBAL *gs, float x0, float y0, float x1, float y1, int z, u64 color)
{
    (void)gs;
    (void)z;
    if (fills < 2) {
        rects[fills].x0 = (int)x0;
        rects[fills].y0 = (int)y0;
        rects[fills].x1 = (int)x1;
        rects[fills].y1 = (int)y1;
        rects[fills].color = color;
    }
    fills++;
}

void gsKit_prim_sprite_texture(GSGLOBAL *gs, GSTEXTURE *texture,
                              float x0, float y0, float u0, float v0,
                              float x1, float y1, float u1, float v1, int z, u64 color)
{
    (void)gs;
    (void)z;
    CHECK(texture->Filter == GS_FILTER_NEAREST);
    CHECK(u0 == 0 && v0 == 0 && u1 == 320 && v1 == 240);
    picture.x0 = (int)x0;
    picture.y0 = (int)y0;
    picture.x1 = (int)x1;
    picture.y1 = (int)y1;
    picture.color = color;
    pictures++;
}

static void present(GSGLOBAL *gs, int logic_frame, int left, int right)
{
    const u64 matte = GS_SETREG_RGBAQ(12, 9, 20, 0x80, 0);
    clears = fills = pictures = 0;
    ps2_game2d_present(gs, logic_frame);
    CHECK(clears == 0);
    CHECK(pictures == 1 && picture.x0 == left && picture.x1 == right);
    CHECK(picture.y0 == 0 && picture.y1 == gs->Height);
    CHECK(picture.color == GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x80, 0));
    if (left > 0) {
        CHECK(fills == 2);
        CHECK(rects[0].x0 == 0 && rects[0].x1 == left);
        CHECK(rects[1].x0 == right && rects[1].x1 == gs->Width);
        CHECK(rects[0].y0 == 0 && rects[0].y1 == gs->Height && rects[0].color == matte);
        CHECK(rects[1].y0 == 0 && rects[1].y1 == gs->Height && rects[1].color == matte);
    } else CHECK(fills == 0);
}

static void test_mode(int height, int left, int right)
{
    GSGLOBAL gs = { 640, height };
    uploads = 0;
    CHECK(ps2_game2d_init(&gs));
    plat_begin_frame();
    plat_clear(11);
    plat_end_frame();
    present(&gs, 1, left, right);
    CHECK(uploads == 1 && uploaded == 0xA21F);

    present(&gs, 1, left, right);
    CHECK(uploads == 1);

    plat_clear(0);
    if (height == 448) {
        present(&gs, 0, left, right);
        CHECK(uploads == 1 && uploaded == 0xA21F);
    }
    present(&gs, 1, left, right);
    CHECK(uploads == 2 && uploaded == 0x8000);
}

int main(void)
{
    test_mode(448, 22, 618);
    test_mode(480, 0, 640);
    if (!failures) puts("PS2 480i and 480p viewport presentation passed");
    return failures != 0;
}
