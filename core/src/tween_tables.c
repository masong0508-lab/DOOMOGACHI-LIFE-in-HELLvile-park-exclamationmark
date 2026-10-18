/* tween_tables.c - the trailer's per-frame keyframe tables (tween.h; owner:
 * S1). Values are CT Appendix A verbatim: the trailer (30 Hz, tk.h TK_FPS)
 * sampled 1:1, so one entry is one game tick. Q8.8 scales (256 = 1.0)
 * unless the comment says px. ROM only (const), about 400 B.
 *
 * Read them with tw_tab(&TW_x, beat_age(label)) or tw_squash(); data that
 * names a table by number (BOP_POP's b, the .dsc compiler's [POP TW_x])
 * goes through tw_table(TWT_x). test_tween.c checks every row.
 */
#include "doomo/doomo.h"
#include "doomo/tween.h"

#define TAB(name, ...) \
    static const int16_t name##_v[] = { __VA_ARGS__ }; \
    const TwTab name = { name##_v, (uint8_t)(sizeof(name##_v) / sizeof(name##_v[0])) }

/* POP (appears in place, starting small) */
TAB(TW_POP_BUBBLE, 102, 307, 236, 266, 256);            /* s04 speech bubble, text from age 2 */
TAB(TW_POP_LETTER, 102, 333, 230, 269, 256);            /* s02 subtitle letter (dy -3 on 0)   */
TAB(TW_POP_SOUL,    77, 333, 236, 269, 256);            /* s11 "SOUL LOST.", s14 labels       */
TAB(TW_POP_LOCKUP,  51, 346, 230, 271, 251, 256);       /* s12 numbers, word 3 f later        */
TAB(TW_POP_EYE,     77, 346, 218, 282, 256);            /* s05 third eye                      */
TAB(TW_POP_MENU,    38, 141, 287, 243, 264, 256);       /* s11 menu window                    */
TAB(TW_POP_BLOB,    51, 320, 230, 269, 256);            /* s15 post-credits blob              */
TAB(TW_POP_BURST,   77, 205, 410, 486, 410, 410);       /* s06 heart burst, ends at 1.6x      */
/* SLAM (lands on the screen, starting large) */
TAB(TW_SLAM_STAMP,   614, 410, 236, 266, 256);          /* s11 "EXORCISM FAILED"              */
TAB(TW_SLAM_COUNTER, 563, 384, 218, 282, 248, 256);     /* s11 xN (x1.2 on the 3rd)           */
TAB(TW_SLAM_STICKER, 410, 320, 236, 266, 256);          /* s02 sticker #1                     */
TAB(TW_SLAM_STICK2,  461, 333, 243, 256);               /* s02 sticker #2                     */
/* DROP impact squash (sx / sy Q8.8, dy px) */
TAB(TW_DROP_TITLE_SX, 353, 307, 230, 246, 264, 256);    /* s02/s15 title letters              */
TAB(TW_DROP_TITLE_SY, 159, 205, 287, 269, 251, 256);
TAB(TW_DROP_TITLE_DY,   0,   0,  -4,  -4,  -1,   0);
TAB(TW_DROP_LET_SX,   333, 287, 241, 264, 256);         /* s01 "HaHaland" letters             */
TAB(TW_DROP_LET_SY,   184, 230, 276, 251, 256);
TAB(TW_DROP_LET_DY,     0,   0,  -3,  -1,   0);
TAB(TW_DROP_SOFT_SX,  312, 282, 243, 251, 261, 256);    /* s14 handheld                       */
TAB(TW_DROP_SOFT_SY,  200, 230, 274, 264, 253, 256);
TAB(TW_DROP_SOFT_DY,    6,   3,  -3,  -2,   0,   0);
TAB(TW_HEART_LAND_SQ,  87,  61,  20);                   /* s01 heart squash sq (/256, + = wide):
                                                           w*(1+sq), h*(1-0.9sq)              */
TAB(TW_DEVICE_DY,       5,   3,  -3,  -3,  -1,   1,   0);  /* s04 device, px                  */
TAB(TW_SIGN_DY,       -14,  -6,   2,   0,  -1,   0);    /* s12 park sign, px                  */
TAB(TW_CLUNK,         210, 236, 271, 261, 256);         /* s14 console sy                     */
TAB(TW_CLAP,          230, 241, 266, 261, 256);         /* s14 sy (sx = 512 - sy)             */
/* motion offsets (px) */
TAB(TW_WINDOW_BOING_DY, 160, 120, 70, 28, -2, -9, -6, 1, 3, 1, 0);  /* s05 window boing      */
TAB(TW_BRAKE_DX,        6, 8, 6, 2, -1, -2, -1, 0);    /* s03 record-scratch brake           */
TAB(TW_ZOOM_WOBBLE_DY,  6, -3, 1, 0);                  /* s05 zoom wobble                    */
TAB(TW_HOP1,           -3, -2, 0, 1, 0, 0);            /* glyph hop at scale 1               */
TAB(TW_HOP2,           -7, -4, 1, 2, 1, 0);            /* glyph hop at scale 2               */
TAB(TW_TAG_DY,         -3, 1, 0);                      /* "(cozy.)" tag                      */
TAB(TW_DEADPAN_DY,      8, 6, 4, 2, 0);                /* s07 deadpan slide-up               */

/* TWT_* -> table (append only: ids are stored in beat scripts and packs) */
static const TwTab *const tw_tables[TWT_COUNT] = {
    &TW_POP_BUBBLE, &TW_POP_LETTER, &TW_POP_SOUL, &TW_POP_LOCKUP, &TW_POP_EYE,
    &TW_POP_MENU, &TW_POP_BLOB, &TW_POP_BURST,
    &TW_SLAM_STAMP, &TW_SLAM_COUNTER, &TW_SLAM_STICKER, &TW_SLAM_STICK2,
    &TW_DROP_TITLE_SX, &TW_DROP_TITLE_SY, &TW_DROP_TITLE_DY,
    &TW_DROP_LET_SX, &TW_DROP_LET_SY, &TW_DROP_LET_DY,
    &TW_DROP_SOFT_SX, &TW_DROP_SOFT_SY, &TW_DROP_SOFT_DY,
    &TW_HEART_LAND_SQ, &TW_DEVICE_DY, &TW_SIGN_DY, &TW_CLUNK, &TW_CLAP,
    &TW_WINDOW_BOING_DY, &TW_BRAKE_DX, &TW_ZOOM_WOBBLE_DY,
    &TW_HOP1, &TW_HOP2, &TW_TAG_DY, &TW_DEADPAN_DY,
};

const TwTab *tw_table(int id)
{
    return (id >= 0 && id < TWT_COUNT) ? tw_tables[id] : NULL;
}
