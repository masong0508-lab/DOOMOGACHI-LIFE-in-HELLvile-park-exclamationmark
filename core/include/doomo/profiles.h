/* profiles.h - MP0: player profiles inside one shared world save (owner: profiles;
 * contract: life_contract.h "PROF", playas.h "Profiles"). Animal Crossing style:
 * profile 0 is the town's representative, the hero, whose state IS the save; up to
 * PROF_GUESTS (3) more people move in, each playing one villager (or, from MP1, a
 * co-op hero). A guest keeps their own name, progress flags, quest bytes, ties to
 * villagers, room and letters here; their money, items and health stay the PLAY
 * pocket of the villager they play. Nothing about "who is playing" is saved apart
 * from PLAY's avatar: the active profile is derived (the one whose villager you
 * control), so a load always agrees with what was on screen.
 *
 * Saved in the 'PROF' chunk: a 32 B header + 96 B per guest (3 guests: 320 B of the
 * 2,336 B contract; the contract's 768 B record is the ceiling for a later layout
 * version). Omitted while there are no guests, so a save without guests is byte-
 * identical to before. The save header's byte 47 bit 3 (PLAYAS_HF_PROFILES) is set
 * while guests live here. The picker is SCENE_PLAYAS (PLAYAS_ARG_PROFILES). */
#ifndef DOOMO_PROFILES_H
#define DOOMO_PROFILES_H

#include "doomo/types.h"
#include "doomo/life_contract.h"

#define PROF_NAME_LEN     DOOMO_NAME_LEN   /* 1..11 printable ASCII, NUL padded */
#define PROF_FLAG_BITS    128              /* progress flags per guest          */
#define PROF_QUEST_BYTES  16               /* quest state bytes per guest       */
#define PROF_REL_MAX      12               /* ties to villagers per guest       */
#define PROF_MAIL_MAX     4                /* letters waiting per guest         */
#define PROF_SZ_REC       96               /* the compact guest record (layout 1) */

enum { PROF_ERR_FULL = -1, PROF_ERR_NAME = -2, PROF_ERR_RESIDENT = -3,
       PROF_ERR_HERO = -4, PROF_ERR_LOCKED = -5, PROF_ERR_HOUSEHOLD = -6 };

/* ---- the roster ---- */
int  prof_guest_count(void);                 /* guests, 0..PROF_GUESTS               */
int  prof_count(void);                       /* profiles: the hero + the guests      */
/* A guest moves in as profile 1..3: name 1..10 printable chars, the villager they
 * play (alive, not the hero, not another guest's), and their hero kind: -1 for a
 * villager, or a filled co-op kind (HERO_COOP_FIRST..+2). A co-op guest passes any
 * resident (use -1) and plays a new housemate made from the hero's look; that takes a
 * household place (PROF_ERR_HOUSEHOLD when full) and moves out with them. Each co-op
 * hero is taken once. Needs play-as to be available (PROF_ERR_LOCKED). Returns the
 * profile index or a PROF_ERR_*. */
int  prof_guest_add(const char *name, int resident, int hero_kind);
int  prof_guest_remove(int profile);
int  prof_rename(int profile, const char *name); /* a guest only: 1 ok, PROF_ERR_NAME, 0 not a guest */
int  prof_is_coop(int profile);               /* 1 when the guest plays a co-op hero */
int  prof_hero_used(int hero_kind);           /* 1 when a guest already plays this co-op hero */         /* moves out; plays back as the hero if active; 1 ok */
const char *prof_name(int profile);          /* "" for an empty profile, the hero's town name for 0 */
int  prof_resident(int profile);             /* the villager a guest plays, -1 none (0: the hero id) */
int  prof_find_resident(int resident);       /* the profile that plays this villager, -1 none        */
int  prof_active(void);                      /* the profile you are playing (= playas_profile())     */
/* Take the pad as this profile: profile 0 switches to the hero, a guest to their
 * villager (playas_switch). 1 on success (or already them), 0 when refused. */
int  prof_switch(int profile);
/* The day's check (sim day_rollover, after playas_check): a guest whose villager
 * is gone moves out, with a toast. Returns guests removed. */
int  prof_check_daily(void);

/* ---- a guest's own state (profile 1..3; profile 0 is the save itself: no-ops) ---- */
int  prof_flag(int profile, int bit);                    /* bit 0..127 */
void prof_flag_set(int profile, int bit, int on);
int  prof_quest(int profile, int i);                     /* i 0..15, a byte */
void prof_quest_set(int profile, int i, int value);
int  prof_rel(int profile, int resident);                /* 0..255, 0 = no tie */
int  prof_rel_set(int profile, int resident, int value); /* 0 removes; 1 ok, 0 when full / bad id */
int  prof_room(int profile);                             /* apartment room id, 0 = none */
void prof_room_set(int profile, int room);
int  prof_mail_send(int profile, int letter);            /* 1 ok, 0 when the box is full */
int  prof_mail_count(int profile);
int  prof_mail_take(int profile);                        /* the oldest letter id, -1 none */

#endif
