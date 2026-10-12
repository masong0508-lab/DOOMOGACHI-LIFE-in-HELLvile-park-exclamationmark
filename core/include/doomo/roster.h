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
 * Modding guide: docs/ROSTER.md.
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

/* ---- roster additions (owner: roster) ----------------------------------- */

/* Text limits (characters, without the NUL). */
#define ROSTER_BIO_MAX 60
#define ROSTER_JOB_MAX 20

/* Story NPC resident ids (the values of doomo_story_npc[], fixed so other
 * modules may use them in const tables; test_roster checks they agree).
 * Resident names are limited to 11 characters, so two of the cast go by
 * shortened names in the roster: "Mayor Giggs" and "Nana Puddin". */
#define ROSTER_ID_MAYOR         0    /* "Mayor Giggs"  cherub, Hillvile      */
#define ROSTER_ID_NANA_PUDDING  1    /* "Nana Puddin"  sheepkin baker        */
#define ROSTER_ID_RIVET         13   /* "Rivet"  imp welder, HELLBORN but
                                        lives in Hillvile (district 0)      */
#define ROSTER_ID_PIP           42   /* "Pip"    bunnykin kid               */
#define ROSTER_ID_FATHER_FUZZ   77   /* "Father Fuzz"  plushie priest        */
#define ROSTER_ID_DR_BLOOM      111  /* "Dr. Bloom"  ghoul doctor           */
#define ROSTER_ID_VEX           666  /* "Vex"  Cloud Nine Vapor, Hellvile    */
#define ROSTER_ID_LORD_VAPULA   999  /* "Lord Vapula"  Duke of the Ninth Cloud */
#define ROSTER_ID_GRUMBLEGUTS   729  /* "Grumbleguts"  imp gatekeeper, an adult who can date and marry */

/* Authored character traits that the story and life sim can use. They are
 * not stored in DoomoResident (and so not in saves): they describe who a
 * premade citizen *is*, see roster_traits(). */
enum {
    ROSTER_T_SOFTIE      = 1,   /* secretly lovely Hellvile citizen: the bio
                                   hints at it; easy Ch.4 redemption       */
    ROSTER_T_KID         = 2,   /* a child                                  */
    ROSTER_T_ELDER       = 4,   /* a granny / grandpa / ancient             */
    ROSTER_T_VAPER       = 8,   /* already vapes: first to act weird in Ch.2 */
    ROSTER_T_HOLY        = 16,  /* church folk: helps with exorcisms        */
    ROSTER_T_LOYALIST    = 32,  /* die-hard Vapula loyalist: hard to redeem */
    ROSTER_T_INFILTRATOR = 64,  /* Hellvile agent working among Hillvile    */
    ROSTER_T_NIGHT_OWL   = 128, /* keeps night hours (night shift, ghosts)  */
    ROSTER_T_TEEN        = 256  /* a teenager: romance only with other teens,
                                   never with adults (KID: no romance at all) */
};
/* ROSTER_T_* bits of a premade citizen (0 for bad ids). */
int roster_traits(int id);
/* Premade record's name ("" for bad ids). */
const char *roster_name(int id);
/* Premade citizen id by name (case-insensitive), or -1. */
int roster_find(const char *name);
/* Roster id of a story NPC (NPC_*), or -1 for a bad index. */
int roster_story_npc(int npc);
/* NPC_* index of a roster id, or -1 if that citizen is not story cast. */
int roster_npc_of(int id);
/* Number of premade citizens with all of the given ROSTER_T_* bits in the
 * given district (0 Hillvile, 1 Hellvile, -1 both). */
int roster_count_traits(int traits, int district);

/* ---- internal: generated by tools/roster_build.py into core/gen/roster_data.c
 * (use the functions above). Bios and jobs are NUL-separated text pools
 * addressed by 16-bit offsets. */
extern const DoomoResident doomo_roster_premade[ROSTER_COUNT];
extern const char     doomo_roster_bio_text[];
extern const uint16_t doomo_roster_bio_ofs[ROSTER_COUNT];
extern const char     doomo_roster_job_text[];
extern const uint16_t doomo_roster_job_ofs[ROSTER_COUNT];
extern const uint16_t doomo_roster_traits[ROSTER_COUNT];

#endif
