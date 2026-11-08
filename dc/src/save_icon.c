/* save_icon.c - the VMU icon and eyecatch of a save (DC4; owner:
 * dreamcast). Portable C (no KallistiOS): the host test draws them too.
 *
 * The Boreagotchi is drawn by the core (pet_render: 32x32, values 0 empty,
 * 1 ink, 2 mid tone, 3 lit LCD) in the form the save holds, so the file
 * manager shows the player's own pet:
 *   icon      32x32, 3 frames (idle, idle, happy; the egg wobbles), on the
 *             tamagotchi's green LCD with a pink rim
 *   eyecatch  72x56, the same LCD at 1.5x on the game's pink, with stripes
 * Both are 4 bits a pixel, the left pixel in the high nibble, and use their
 * own 16 ARGB4444 colours (the VMS format; vms.h).
 */
#include <string.h>
#include "vmusave.h"
#include "doomo/palette.h"
#include "doomo/pet.h"
#include "doomo/town.h"

int dc_vmu_pet_form(void)
{
    const DoomoPet *p = &doomo_town.pet;
    if (!pet_exists() || p->stage == PET_STAGE_EGG) return -1;
    if (p->stage == PET_STAGE_DEAD) return -2;
    return p->form < PET_FORM_COUNT ? (int)p->form : -1;
}

const char *dc_vmu_desc_long(int slot)
{
    return slot == 0 ? "BOREagotchi LIFE! - story" :
           slot == 1 ? "BOREagotchi LIFE! - free play" : "BOREagotchi LIFE! - settings";
}

static uint16_t argb4444(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(0xF000u | ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4));
}

static uint16_t pal4444(uint8_t index)
{
    uint8_t r, g, b;
    palette_rgb(index, &r, &g, &b);
    return argb4444(r, g, b);
}

/* colours: 0 transparent, 1 LCD lit, 2 mid tone, 3 ink, 4 pink, 5 dark pink */
enum { C_CLEAR, C_LIT, C_MID, C_INK, C_PINK, C_PINK_D };

static void colours(uint16_t pal[16])
{
    memset(pal, 0, 16 * sizeof(uint16_t));
    pal[C_CLEAR] = 0x0000;
    pal[C_LIT] = pal4444(PAL_UI_LCD);
    pal[C_MID] = argb4444(104, 124, 76);     /* the core's LCD tones (pet_gfx.c) */
    pal[C_INK] = argb4444(44, 56, 40);
    pal[C_PINK] = pal4444(PAL_UI_PINK);
    pal[C_PINK_D] = pal4444(PAL_UI_PINK_D);
}

static uint8_t lcd_colour(uint8_t v)
{
    return v == 1 ? C_INK : v == 2 ? C_MID : C_LIT;     /* 0 and 3: lit LCD */
}

static void put4(uint8_t *bits, int stride, int x, int y, uint8_t c)
{
    uint8_t *b = bits + y * (stride / 2) + x / 2;
    if (x & 1) *b = (uint8_t)((*b & 0xF0) | (c & 15));
    else       *b = (uint8_t)((*b & 0x0F) | (c << 4));
}

static int frame_of(int form, int i)
{
    static const int pet_frames[DC_VMU_FRAMES] = { PET_ANIM_IDLE0, PET_ANIM_IDLE1, PET_ANIM_HAPPY };
    static const int egg_frames[DC_VMU_FRAMES] = { 0, 1, 0 };
    return form < 0 ? egg_frames[i] : pet_frames[i];
}

void dc_vmu_icon(int form, uint16_t pal[16], uint8_t icon[DC_VMU_FRAMES * VMS_ICON_SIZE])
{
    uint8_t px[PET_SPR * PET_SPR];
    int f, x, y;
    colours(pal);
    for (f = 0; f < DC_VMU_FRAMES; f++) {
        uint8_t *bits = icon + f * VMS_ICON_SIZE;
        pet_render(form, frame_of(form, f), px);
        for (y = 0; y < 32; y++)
            for (x = 0; x < 32; x++) {
                int rim = x == 0 || y == 0 || x == 31 || y == 31;
                int corner = (x == 0 || x == 31) && (y == 0 || y == 31);
                uint8_t c = corner ? C_CLEAR : rim ? C_PINK_D : lcd_colour(px[y * PET_SPR + x]);
                put4(bits, 32, x, y, c);
            }
    }
}

void dc_vmu_eyecatch(int form, uint16_t pal[16], uint8_t ec[VMS_EC16_SIZE - 32])
{
    uint8_t px[PET_SPR * PET_SPR];
    int x, y;
    colours(pal);
    pet_render(form, frame_of(form, 2), px);
    for (y = 0; y < 56; y++)
        for (x = 0; x < 72; x++) {
            uint8_t c = (y == 1 || y == 54) ? C_PINK_D : C_PINK;
            /* the LCD: 50x50 with a dark rim, the pet 48x48 inside (1.5x) */
            if (x >= 11 && x < 61 && y >= 3 && y < 53) {
                int lx = x - 12, ly = y - 4;
                if (x == 11 || x == 60 || y == 3 || y == 52) c = C_PINK_D;
                else c = lcd_colour(px[(ly * 2 / 3) * PET_SPR + lx * 2 / 3]);
            }
            put4(ec, 72, x, y, c);
        }
}
