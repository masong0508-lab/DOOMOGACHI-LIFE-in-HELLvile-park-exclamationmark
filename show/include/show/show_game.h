/* show_game.h - the show player's entry points and fixed numbers (spec 2.1). */
#ifndef SHOW_GAME_H
#define SHOW_GAME_H

#include <stdint.h>

/* the 16:9 picture inside the NTSC 320x240 display (open matte) */
#define SHOW_CANVAS_W   320
#define SHOW_CANVAS_H   180
#define SHOW_CANVAS_Y   30          /* show y + 30 = screen y            */
#define SHOW_WALL_X     160         /* the Wall line                     */
#define SHOW_SAFE_X     16          /* title-safe (90 %), left / right   */
#define SHOW_SAFE_Y     9           /* title-safe, top / bottom          */
#define SHOW_SEED       0x00101101u /* episode-independent default seed  */
#define SHOW_BUILD_ID   "wp2"

/* show_chart.c: the boot test chart (M0-T6, AT2.1) */
void show_chart_init(void);
void show_chart_draw(uint32_t frame);

#ifdef SHOW_SELFTEST
/* show_selftest.c: AT1.3; returns 1 while it owns the frame */
void show_selftest_init(void);
int  show_selftest_frame(uint32_t frame);
#endif

#endif
