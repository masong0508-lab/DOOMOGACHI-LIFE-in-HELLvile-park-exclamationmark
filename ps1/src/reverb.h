/* reverb.h - SPU reverb presets for the PS1 (MS-S4, docs/MUSIC_STUDIO_ROADMAP.md E).
 * The reverb work area is the top of SPU RAM, 0x7e000-0x7ffff (8,192 B), which nothing else
 * uses. The BIOS presets are laid out ending at 0x80000, so only the smallest one fits
 * ("Studio Small", 8,000 B, mBASE 0xFC18 = 0x7e0c0). It is the room for the studio; the hall
 * (0xADE0 B) would have to take space from the part store, so PERFORM uses the same room with a
 * higher send. Register order = the 32 halfwords at 0x1F801DC0 (psx-spx "reverb registers").
 * Host-tested by ps1/harness/reverb_test.c. */
#ifndef PS1_REVERB_H
#define PS1_REVERB_H
#include <stdint.h>
#define PS1_REVERB_AREA_START 0x7e000u
#define PS1_REVERB_AREA_END   0x80000u
#define PS1_REVERB_REGS       32
#define PS1_REVERB_REG_BASE   0x1F801DC0u   /* dAPF1 .. vRIN */
#define PS1_REVERB_MBASE_REG  0x1F801DA2u   /* work area start / 8 */
#define PS1_REVERB_VOL_L_REG  0x1F801D84u
#define PS1_REVERB_VOL_R_REG  0x1F801D86u
#define PS1_REVERB_EON1_REG   0x1F801D98u   /* voices 0-15 */
#define PS1_REVERB_EON2_REG   0x1F801D9Au   /* voices 16-23 */
#define PS1_REVERB_SPUCNT_REG 0x1F801DAAu   /* bit 7 = reverb master enable */
#define PS1_REVERB_SPUCNT_EN  0x0080u
enum { REVERB_OFF, REVERB_ROOM, REVERB_HALL, REVERB_COUNT };   /* HALL = ROOM with a higher send */
typedef struct Ps1Reverb {
    uint16_t mbase;           /* work area start / 8 */
    uint16_t size_bytes;      /* the preset's work area size */
    uint16_t reg[PS1_REVERB_REGS];
} Ps1Reverb;
static const Ps1Reverb ps1_reverb_studio_small = {
    0xFC18u, 0x1F40u,
    { 0x0033, 0x0025, 0x70F0, 0x4FA8, 0xBCE0, 0x4410, 0xC0F0, 0x9C00,
      0x5280, 0x4EC0, 0x03E4, 0x031B, 0x03A4, 0x02AF, 0x0372, 0x0266,
      0x031C, 0x025D, 0x025C, 0x018E, 0x022F, 0x0135, 0x01D2, 0x00B7,
      0x018F, 0x00B5, 0x00B4, 0x0080, 0x004C, 0x0026, 0x8000, 0x8000 }
};
/* the send level (vLOUT / vROUT, signed 15 bit) per mode; the tone voices' EON bits are 0..3 */
static int16_t ps1_reverb_level(int mode)
{
    return (int16_t)(mode == REVERB_ROOM ? 0x1800 : mode == REVERB_HALL ? 0x2400 : 0);
}
#endif
