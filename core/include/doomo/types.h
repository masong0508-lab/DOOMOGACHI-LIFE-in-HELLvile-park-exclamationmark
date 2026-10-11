/*
 * DOOMOGACHI LIFE in HELLvile Park!
 * types.h - basic types, fixed point, global limits.
 *
 * The core is portable C99 shared by the GBA ROM, the PS1 disc and the
 * native test runner. Rules (see ARCHITECTURE.md):
 *   - no floating point anywhere (GBA and PS1 have no FPU)
 *   - no malloc; all state is static
 *   - only <stdint.h>, <stddef.h>, <string.h> from libc
 *   - all text is 7-bit ASCII plus the icon glyphs in font.h
 */
#ifndef DOOMO_TYPES_H
#define DOOMO_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ---- limits ------------------------------------------------------------ */
#define DOOMO_DISTRICT_SIZE   500   /* homes per district                */
#define DOOMO_MAX_RESIDENTS   1000  /* ids 0..499 Hillvile (good side of
                                       Hellvile Park), 500..999 Hellvile
                                       (bad side)                         */
#define DOOMO_NAME_LEN        12    /* 11 chars + NUL                     */
#define DOOMO_MAX_EDGES       2048  /* relationship edges (sparse graph)  */
#define DOOMO_MAX_WEAPONS     24    /* welded weapons in the armory       */
#define DOOMO_MAX_ITEMS       64    /* inventory item kinds               */
#define DOOMO_MAX_GRAVES      64    /* graveyard plots                    */
#define DOOMO_STORY_FLAGS     256   /* story bit flags                    */
#define DOOMO_TICK_HZ         30    /* core logic runs at a fixed 30 Hz   */

/* ---- fixed point ------------------------------------------------------- */
typedef int32_t fix16;              /* 16.16 */
#define FIX_ONE        ((fix16)0x10000)
#define FIX_HALF       ((fix16)0x8000)
#define INT2FIX(i)     ((fix16)((i) << 16))
#define FIX2INT(f)     ((int32_t)((f) >> 16))
#define FIXMUL(a, b)   ((fix16)(((int64_t)(a) * (int64_t)(b)) >> 16))
#define FIXDIV(a, b)   ((fix16)((((int64_t)(a)) << 16) / (b)))

/* Angles: 4096 units per full turn. 0 = +X (east), 1024 = +Y (south,
 * map rows grow downward), 2048 = west, 3072 = north. */
#define DOOMO_ANG_FULL  4096
#define DOOMO_ANG_MASK  4095
#define DOOMO_ANG_90    1024

#define DOOMO_MIN(a, b) ((a) < (b) ? (a) : (b))
#define DOOMO_MAX(a, b) ((a) > (b) ? (a) : (b))
#define DOOMO_CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))
#define DOOMO_COUNTOF(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* Compile-time assert usable in C99. */
#define DOOMO_STATIC_ASSERT(cond, name) \
    typedef char doomo_static_assert_##name[(cond) ? 1 : -1]

/* An 8bpp indexed image. Index 0 is always transparent.
 * Pixels are row-major, w*h bytes, indices into the master palette
 * (palette.h). `uid` MUST change whenever the pixel contents change so
 * that platforms which upload images to VRAM (PS1) can cache by uid.
 * Use doomo_image_new_uid() to get a fresh uid. */
typedef struct DoomoImage {
    uint16_t w, h;
    const uint8_t *pix;
    uint32_t uid;
} DoomoImage;

uint32_t doomo_image_new_uid(void);

#endif
