/* home.h - the dream house: homes, rooms, stats, grades, phases, the
 * SCENE_HOME argument encoding and the 'HOME' save chunk layout.
 *
 * CONTRACT (docs/design/story/SPEC.md sec.4, R9, R10, C9, C14): written by S0,
 * maintained by H1 (add-only). State and the chunk: core/src/home.c (H1);
 * the scene: core/src/home_draw.c (H4). Story code reaches the house only
 * through scene_push(&SCENE_HOME, arg) and house_api.h.
 */
#ifndef DOOMO_HOME_H
#define DOOMO_HOME_H

#include "types.h"
#include "furn.h"
#include "save_ext.h"

/* The homes the player can own (HOME_ROOM_* below are their room records). */
enum { HOME_COTTAGE, HOME_TOWERS_7B, HOME_PITS_FLAT, HOME_PARK_PLOT, HOME_COUNT };

/* Room records in the save (9, SPEC R10): the cottage's six rooms, then one
 * room each for the other homes. The Park Plot is one 10x8 open-kind room
 * where FF_WALL pieces may go on any grid edge (post-game Build). */
enum { HOME_ROOM_LIVING, HOME_ROOM_BEDROOM, HOME_ROOM_KITCHEN, HOME_ROOM_CELLAR,
       HOME_ROOM_GARDEN, HOME_ROOM_ANNEX, HOME_ROOM_TOWERS_7B, HOME_ROOM_PITS_FLAT,
       HOME_ROOM_PARK_PLOT, HOME_ROOM_COUNT };
#define HOME_COTTAGE_ROOMS 6
#define HOME_OBJ_MAX       40       /* pieces per room                        */

/* The four stats plus Barricade (HF sec.4.1), each 0..100. These are not
 * resident.h's STAT_* needs. */
typedef enum HomeStat { HS_COMFORT, HS_SPOOK, HS_STYLE /* Harmony from Ch4 */,
                        HS_LIGHT, HS_BARRICADE, HS_COUNT } HomeStat;

/* Grades (the stamp). */
enum { HG_NONE, HG_COSY, HG_LIT, HG_HELD, HG_HOME, HG_ETERNAL, HG_COUNT };

/* The house phase across the story. The single source is DoomoStry.home_phase
 * (story.h); HOME does not duplicate it (SPEC sec.4). */
enum { HP_COSY, HP_HAUNTED, HP_BARRICADED, HP_EMPTY, HP_OPEN, HP_DREAM, HP_COUNT };

/* ---- SCENE_HOME arg (SPEC C14) -------------------------------------------
 *   -(1 + HOME_*)   the player's own home k (-1 = the cottage)
 *   2 + id          visit citizen `id` (0..999)
 *   0x4000 | mode   story modes (HSM_*)
 *   0, 1            reserved, refused                                       */
#define HOME_ARG_OWN(k)      (-(1 + (k)))
#define HOME_ARG_VISIT(id)   (2 + (id))
#define HOME_ARG_STORY(mode) (0x4000 | (mode))
#define HOME_ARG_IS_OWN(a)   ((a) < 0 && (a) >= -HOME_COUNT)
#define HOME_ARG_IS_VISIT(a) ((a) >= 2 && (a) < 0x4000)
#define HOME_ARG_IS_STORY(a) ((a) >= 0x4000 && (a) < 0x8000)
enum { HSM_TUTORIAL, HSM_GOAL, HSM_HAUNT_NIGHT, HSM_DEFEND, HSM_DECORATE_FOR,
       HSM_OPEN_HOUSE, HSM_PHOTO, HSM_COUNT };

/* ---- the 'HOME' chunk, version 2 (SPEC R9, R10, C9) ----------------------
 * HF sec.9.2's layout with 9 room records, owned[192], seen_new[48], a visitor
 * log, decorate_for progress, a pending-gag queue and cached results that
 * house_api.c serves without the home group (C6). size() reports exactly
 * what write() emits; rooms are written with their real nobj. */
#define HOME_CHUNK_TAG        SAVE_EXT_TAG('H', 'O', 'M', 'E')
#define HOME_CHUNK_VERSION    2
#define HOME_SZ_HEADER        8
#define HOME_SZ_ROOM_HEADER   10
#define HOME_SZ_OBJ           6
#define HOME_SZ_ROOM_MAX      (HOME_SZ_ROOM_HEADER + HOME_OBJ_MAX * HOME_SZ_OBJ)   /* 250 */
#define HOME_SZ_OWNED         (FURN_MAX / 2)                                       /* 192: nibble counts */
#define HOME_SZ_SEEN_NEW      (FURN_MAX / 8)                                       /* 48: "NEW!" bits */
#define HOME_GIFTS            32
#define HOME_SZ_GIFTS         (HOME_GIFTS * 4)
#define HOME_SZ_VISITED       125                                                  /* 1000 bits */
#define HOME_SZ_HAUNT         16
#define HOME_BLUEPRINTS       2
#define HOME_SZ_BLUEPRINTS    (HOME_BLUEPRINTS * HOME_SZ_ROOM_MAX)
#define HOME_PHOTOS           16
#define HOME_SZ_PHOTOS        (HOME_PHOTOS * 8)
#define HOME_SZ_WANTS         4
#define HOME_SZ_VISITOR_LOG   8
#define HOME_SZ_DECORATE_FOR  1
#define HOME_PENDING_GAGS     4
#define HOME_SZ_PENDING_GAG   (HOME_PENDING_GAGS * 2)
/* cached results: 5 stats + 9 room grades + decorate_for + defence */
#define HOME_SZ_CACHE         (HS_COUNT + HOME_ROOM_COUNT + 2)
#define HOME_CHUNK_WORST \
    (HOME_SZ_HEADER + HOME_ROOM_COUNT * HOME_SZ_ROOM_MAX + HOME_SZ_OWNED + \
     HOME_SZ_SEEN_NEW + HOME_SZ_GIFTS + HOME_SZ_VISITED + HOME_SZ_HAUNT + \
     HOME_SZ_BLUEPRINTS + HOME_SZ_PHOTOS + HOME_SZ_WANTS + HOME_SZ_VISITOR_LOG + \
     HOME_SZ_DECORATE_FOR + HOME_SZ_PENDING_GAG + HOME_SZ_CACHE)     /* 3,432 B */
/* In free play the chunk is this small while nobody has moved in (R13). */
#define HOME_CHUNK_EMPTY      10

#endif
