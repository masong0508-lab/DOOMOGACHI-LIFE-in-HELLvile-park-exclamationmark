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

/* ======================================================================
 * life additions (owner: life; additive)
 * ==================================================================== */

/* Forms: form = line * 4 + (stage - PET_STAGE_BABY) for babies..adults:
 *   CUTE   0 Mochiblob   1 Puffling   2 Sprinkle    3 Cupcakeling
 *   ANGEL  4 Halopup     5 Seraphuff  6 Choirbun    7 Archangelo
 *   FERAL  8 Scruffle    9 Grumblin  10 Feralfloof 11 Wildbeastie
 *   DEMON 12 Impkin     13 Hornlet   14 Hellmuffin 15 Demogorgoo
 * At each evolution the line is decided again: ANGEL if holiness >= 75
 * with few care mistakes, DEMON if holiness <= 25, FERAL after many care
 * mistakes, CUTE otherwise. Snacks, unfair scolding and ignored tantrums
 * make it devilish; meals, cleaning promptly, fair discipline and lights
 * out at bedtime make it holy.
 *
 * DoomoPet packing (town.h): `asleep` bit0 asleep, bit1 lights off, bit2
 * calling for attention, bit3 the call is a fake (a tantrum: discipline
 * it!), bits4-7 minutes the call has waited / 4. `sick` bit0 sick, bits1-3
 * hours untreated, bits4-5 doses still needed. A pet with an empty name
 * does not exist yet (pet_new() lays an egg). */
#define PET_FORM(line, stage) ((line) * 4 + (stage) - PET_STAGE_BABY)
#define PET_HEARTS 4

/* Minutes each stage lasts before its evolution check (egg -> baby ...). */
#define PET_EGG_MINUTES    5
#define PET_BABY_MINUTES   60
#define PET_CHILD_MINUTES  1440
#define PET_TEEN_MINUTES   2880

/* Animation frames for pet_image() / pet_render(). */
enum { PET_ANIM_IDLE0, PET_ANIM_IDLE1, PET_ANIM_HAPPY, PET_ANIM_EAT,
       PET_ANIM_SAD, PET_ANIM_SLEEP, PET_ANIM_SICK, PET_ANIM_ANGRY,
       PET_ANIM_COUNT };
/* Egg frames: 0/1 wobble, 2 cracked, 3 hatching. Dead: 0/1 floating. */

int  pet_exists(void);               /* has a name (an egg or more)       */
int  pet_calling(void);              /* attention icon lit                */
int  pet_call_is_tantrum(void);      /* a fake call: discipline it        */
int  pet_is_asleep(void);
int  pet_lights_off(void);
int  pet_is_sick(void);
/* Result of the last pet_feed(): 1 ate, 0 refused (full / sick / asleep). */
int  pet_last_fed(void);
/* The line the next evolution would pick right now (a forecast). */
int  pet_line_forecast(void);
/* Force the evolution check now (story beats, tests). Returns 1 if the
 * pet changed form. */
int  pet_evolve_now(void);
/* 1 if this form has ever been raised (its UL_CAT_PET unlock is owned). */
int  pet_form_seen(int form);
/* Human readable combat buff of the adult form ("+15% damage, holy"). */
const char *pet_buff_text(void);
extern const char *const doomo_pet_line_names[4];
extern const char *const doomo_pet_stage_names[PET_STAGE_DEAD + 1];

/* Chunky LCD art, 32x32, generated in code: values 0 empty, 1 ink,
 * 2 mid tone, 3 lit (LCD background, opaque). form -1 = the egg,
 * form -2 = the departed pet's little ghost. */
#define PET_SPR 32
void pet_render(int form, int frame, uint8_t *out);
/* The same as a DoomoImage in LCD colours (small rotating cache). */
const DoomoImage *pet_form_image(int form, int frame);

#endif
