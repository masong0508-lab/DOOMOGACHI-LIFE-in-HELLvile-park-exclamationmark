/* show_core.h - lane 1's private glue between the player core files.
 * Not part of the plug-in API (show/include/show/show_api.h is). */
#ifndef SHOW_CORE_H
#define SHOW_CORE_H

#include "doomo/doomo.h"
#include "doomo/camfx.h"
#include "doomo/beat.h"
#include "show/show_api.h"
#include "show/show_mbox.h"

/* show_load.c: the episode and the bound chunk */
int  show_load_episode(int episode);            /* 1 = loaded (index in RAM) */
const ShwEpHeader *show_episode(void);
const ShwShotIndex *show_shot_index(int i);
int  show_load_chunk(int shot);                 /* 1 = bound                 */
void show_chunk_bind_mem(const uint8_t *chunk, uint32_t bytes); /* tests     */

/* show_stage.c: the plane table and the plane loop */
void show_stage_bind(void);                     /* sort the shot's planes    */
void show_stage_prep(void);                     /* kind->prep per plane      */
void show_stage_draw(int t);                    /* kind->draw by order       */
void show_images_prep(void);                    /* decode SHWR_IMAGE         */
uint32_t show_plane_vis_mask(void);

/* show_purity.c */
void show_purity_reset(void);

/* show_ext.c */
void show_ext_event(const BeatEv *ev);
void show_ext_reset(void);
void show_ext_hash(ShowCrc *c);

/* show_hash.c */
uint32_t show_p1_frame(void);                   /* the frame's P1            */
void show_p4_reset_frame(void);
void show_p4_fire_due(int t);                   /* SHWR_AUDIO_K at t         */

/* show_stats.c */
void show_stats_begin_frame(void);
void show_stats_mark_update(void);
void show_stats_mark_draw(void);
void show_stats_publish(void);
void show_rng_seed(uint32_t s);
uint32_t show_rng_state(void);

/* the current shot, written by show_game.c */
extern ShowShot g_show_shot;
extern int g_show_drawing;

#endif
