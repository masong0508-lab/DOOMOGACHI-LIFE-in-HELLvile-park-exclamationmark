/* weld_int.h - shared by Rivet's workshop (workshop.c) and the welding
 * minigame (weld_game.c) (owner: action). Not part of the contract. */
#ifndef DOOMO_WELD_INT_H
#define DOOMO_WELD_INT_H

#include "doomo/doomo.h"

/* The design on the welding table. SCENE_WELD_GAME welds it. */
extern DoomoWeapon weld_pending;
/* Result of the last minigame: quality 0..100, or -1 if cancelled. */
extern int weld_game_result;
/* Put the welded weapon into the armory, pay the materials, count it.
 * Returns the armory slot or -1 (no room / can't pay). */
int  workshop_store_weld(int quality);
/* 1 if the player can pay for the pending design (and every part is
 * unlocked and there is room in the armory). why = short reason. */
int  workshop_can_weld(const char **why);

#endif
