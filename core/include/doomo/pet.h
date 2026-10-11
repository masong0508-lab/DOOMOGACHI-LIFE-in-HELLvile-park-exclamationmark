/* pet.h - the Doomogachi: a tamagotchi you raise (owner: life).
 * Egg -> baby -> child -> teen -> adult. Care quality and holiness decide
 * the evolution line: ANGEL (very holy), CUTE (good care), FERAL (neglect),
 * DEMON (corrupted). The pet's screen is an LCD-green tamagotchi panel. */
#ifndef DOOMO_PET_H
#define DOOMO_PET_H

#include "types.h"

enum { PET_STAGE_EGG, PET_STAGE_BABY, PET_STAGE_CHILD, PET_STAGE_TEEN,
       PET_STAGE_ADULT, PET_STAGE_DEAD };
enum { PET_LINE_CUTE, PET_LINE_ANGEL, PET_LINE_FERAL, PET_LINE_DEMON };
#define PET_FORM_COUNT 16

extern const char *const doomo_pet_form_names[PET_FORM_COUNT];

void pet_new(const char *name);
void pet_tick_minute(void);          /* called by sim per game minute     */
void pet_feed(int snack);            /* 0 meal, 1 snack                   */
void pet_play_result(int won);
void pet_clean(void);
void pet_medicine(void);
void pet_discipline(void);
void pet_lights(int off);
int  pet_line(void);                 /* PET_LINE_* */
/* Combat buff granted by the adult form (actor.c reads it). */
int  pet_combat_buff(void);
/* 32x32 LCD-style sprite for the current form & animation frame. */
const DoomoImage *pet_image(int frame);

#endif
