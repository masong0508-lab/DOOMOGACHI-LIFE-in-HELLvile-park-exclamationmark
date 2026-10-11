/* roster.h - the 1000 premade citizens (owner: roster).
 *
 *   ids   0..499  the 500 good citizens of Hillvile (good side of the park)
 *   ids 500..999  the 500 bad citizens of Hellvile (bad side of the park)
 *
 * Authored as human-editable text in core/data/roster/ (one citizen per
 * line/record) and compiled by tools/roster_build.py into const ROM data
 * (core/gen/roster_data.c). Every citizen is distinct: unique name, a
 * look built with the full creator option set, personality, voice,
 * likes, catchphrase, birthday, a one-line bio, and pre-seeded
 * relationships (families, couples, rivals, cross-border crushes).
 * Save files store residents as deltas against these records.
 */
#ifndef DOOMO_ROSTER_H
#define DOOMO_ROSTER_H

#include "types.h"
#include "resident.h"
#include "town.h"

#define ROSTER_COUNT DOOMO_MAX_RESIDENTS

const DoomoResident *roster_premade(int id);   /* 0..999 */
const char *roster_bio(int id);                /* <= 60 chars */
const char *roster_job(int id);                /* "Baker", "Imp Accountant" */

/* Pre-seeded relationships between premade citizens. */
extern const DoomoEdge doomo_roster_edges[];
extern const uint16_t  doomo_roster_edge_count;

/* Fixed story cast (ids into the roster). */
enum {
    NPC_MAYOR,        /* Mayor Gigglesworth - cherub mayor of Hillvile     */
    NPC_FATHER_FUZZ,  /* plushie priest, does the exorcisms                */
    NPC_RIVET,        /* imp welder who ran away from Hellvile; workshop   */
    NPC_DR_BLOOM,     /* ghoul doctor, hospital                            */
    NPC_NANA_PUDDING, /* baker, gives the Doomogachi egg                   */
    NPC_PIP,          /* kid who lost their bunny plush (first quest)       */
    NPC_VEX,          /* owner of Cloud Nine Vapor - a Hellvile infiltrator */
    NPC_LORD_VAPULA,  /* Duke of the Ninth Cloud, demo final boss          */
    NPC_COUNT
};
extern const uint16_t doomo_story_npc[NPC_COUNT];

#endif
