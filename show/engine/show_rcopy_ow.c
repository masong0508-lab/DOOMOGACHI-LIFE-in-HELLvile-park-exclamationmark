/* show_rcopy_ow.c - the game's overworld renderer, compiled for the show
 * (owner: show, ENGINE lane 3; spec 4.3, 8.2, 8.3).
 *
 * ps1/src/overworld.c is compiled here a second time, unchanged (the file
 * is #included, never copied or edited), with its one entry point renamed.
 * Sharing its translation unit gives the show the renderer's private
 * camera state, which the game smooths from frame to frame (a glide with a
 * look-ahead toward the walking direction). A show camera is keyed: every
 * frame must show exactly the keyed focus whatever came before (a cut, a
 * seek, the previous shot), so show_ow_render() drops that history first.
 * The game's own plat_overworld() stays in ps1/src/overworld.c, unused by
 * the show (--gc-sections drops it).
 */
#include "doomo/doomo.h"

#define plat_overworld show_ow_render_game
#include "../../ps1/src/overworld.c"
#undef plat_overworld

void show_ow_render(const DoomoOverworldView *v);

void show_ow_render(const DoomoOverworldView *v)
{
    s_have_cam = 0;             /* snap: the focus is exactly v->cx, v->cy */
    s_leadx = s_leady = 0;
    show_ow_render_game(v);
}
