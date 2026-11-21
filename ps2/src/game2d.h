/* PS2-1 experimental shared-core software surface + GS upload. */
#ifndef DOOMO_PS2_GAME2D_H
#define DOOMO_PS2_GAME2D_H
#include <gsKit.h>
#include <stdint.h>
enum { PS2_GAME_W=320, PS2_GAME_H=240 };
extern uint8_t ps2_indexed_fb[PS2_GAME_W*PS2_GAME_H];
int ps2_game2d_init(GSGLOBAL *gs);
void ps2_game2d_present(GSGLOBAL *gs, int upload_frame);
/* Debug telemetry: read-only, sampled sparingly; checksum is for
 * screenshot/boot diagnostics, not the exact upload-suppression gate. */
void ps2_game2d_probe(uint32_t *ticks,uint32_t *uploads,uint32_t *frame_hash);
#endif
