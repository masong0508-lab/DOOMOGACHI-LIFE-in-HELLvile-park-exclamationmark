/* PS2 renderer common surface, matching native reference rendering.
 * World + raycaster share the current game's 320x240 pixel buffer.
 * This does NOT allocate an additional framebuffer. */
#ifndef DOOMO_PS2_FRAME_REF_H
#define DOOMO_PS2_FRAME_REF_H
#include <stdint.h>
#include "doomo/doomo.h"
#include "doomo/texbank.h"
#include "game2d.h"
#define nat_fb ps2_indexed_fb
#define nat_w PS2_GAME_W
#define nat_h PS2_GAME_H
#define NAT_MAX_W PS2_GAME_W
#define NAT_MAX_H PS2_GAME_H
static inline int nat_bayer(int x, int y)
{
    static const uint8_t b[4][4]={{0,8,2,10},{12,4,14,6},
                                  {3,11,1,9},{15,7,13,5}};
    return b[y&3][x&3];
}
#endif
