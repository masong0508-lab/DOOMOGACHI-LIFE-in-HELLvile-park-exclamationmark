/* overworld_int.h - internals of SCENE_STORY, the overworld (owner: story):
 * overworld.c (scene, player, camera, collision, events, pause panel),
 * overworld_npc.c (citizens, the cast, demon packs, props),
 * overworld_fx.c (billboard markers, particles, fireworks, HUD, minimap).
 */
#ifndef DOOMO_OVERWORLD_INT_H
#define DOOMO_OVERWORLD_INT_H

#include "story_int.h"

/* Positions are world units x 16 ("q4"): a 64x64-cell map is 65536 wide. */
#define OWQ 16
#define OW_CELL (DOOMO_CELL_UNITS * OWQ)
#define OW_NPC_MAX 40
#define OW_PROP_MAX 256
#define OW_BB_MAX 120

enum { NK_RESIDENT, NK_DEMON };
enum { NS_IDLE, NS_WALK, NS_TALK };

typedef struct OwNpc {
    int16_t id;              /* resident id, or THING_* for demons          */
    uint8_t kind;            /* NK_*                                         */
    uint8_t flags;           /* NPCF_*                                       */
    int32_t x, y;            /* q4 world units                               */
    int32_t hx, hy;          /* home                                         */
    int32_t tx, ty;          /* walk target                                  */
    uint8_t dir, state;      /* dir 0 down 1 left 2 right 3 up               */
    uint16_t timer;
    uint8_t nest;            /* demons: nest index                           */
    uint8_t emote;           /* EMO_*                                        */
    uint8_t phase;           /* animation offset                             */
    uint8_t speed;           /* q4 units per tick                            */
} OwNpc;

typedef struct OwProp {
    int16_t x, y;            /* world units (cell centre)                    */
    uint8_t type;            /* THING_*                                      */
    uint8_t r;               /* collision radius (0 = walk through)          */
} OwProp;

enum { EMO_NONE, EMO_TALK, EMO_HEART, EMO_NOTE, EMO_ZZZ, EMO_SWEAT, EMO_SKULL };

typedef struct OwState {
    int32_t px, py;          /* player, q4                                   */
    uint8_t dir, moving, running;
    uint16_t anim;
    int16_t push_t;          /* ticks spent pushing into a door             */
    int8_t last_cx, last_cy; /* cell for trigger edges                      */
    int32_t camx, camy;      /* camera centre, q4                            */
    int32_t trail_x[16], trail_y[16];
    uint8_t trail_i;
    int32_t petx, pety;
    uint8_t visit;           /* free play stroll: no plot                    */
    uint8_t map;
    uint8_t fade;            /* 0..15 black                                  */
    uint8_t shake, flash, flash_col;
    uint16_t banner_t;
    const char *banner;
    const char *region;
    uint8_t pause, pause_sel;
    uint16_t pause_t;
    uint8_t minimap;
    uint8_t started;
    int16_t near_npc;        /* index into ow_npcs or -1                     */
    int16_t near_prop;       /* index into ow_props or -1                    */
    int16_t near_door;       /* place id or 0                                */
    int16_t near_spot;       /* side quest spot or -1                        */
    uint16_t hint_t;
    uint32_t t;
} OwState;

extern OwState ow;

/* (integration, memory budget) The per-map state that is big but can be
 * rebuilt from the map, the day and the story - the citizens, the props
 * and the generated bubble / mini-map images (~5.6 KB) - lives in the
 * shared scene scratch (scene.h, tag OWLD), not in .bss. Any scene the
 * overworld pushes may take the scratch; ow_scratch_claim() notices that
 * and the overworld respawns the cast (it did so after every pushed scene
 * anyway). The per-frame billboard list is the shared doomo_frame_bb. */
/* overworld_fx.c's generated images: quest markers, emote bubbles and the
 * mini-map (2x2 cells per pixel, drawn at 2x so it keeps its size) */
#define OWFX_MK_W 13
#define OWFX_MK_H 15
#define OWFX_MK_COUNT 3
#define OWFX_EM_W 11
#define OWFX_EM_H 12
#define OWFX_MM_W (DOOMO_MAP_MAX_W / 2)
#define OWFX_MM_H (DOOMO_MAP_MAX_H / 2)
typedef struct OwFxPix {
    uint8_t mk[OWFX_MK_COUNT][OWFX_MK_W * OWFX_MK_H];
    uint8_t em[8][OWFX_EM_W * OWFX_EM_H];
    uint8_t mm[OWFX_MM_W * OWFX_MM_H];
} OwFxPix;
typedef struct OwScratch {
    OwNpc npcs[OW_NPC_MAX];
    OwProp props[OW_PROP_MAX];
    OwFxPix fx;
} OwScratch;
extern OwScratch *ow_scr;
#define ow_npcs (ow_scr->npcs)
#define ow_props (ow_scr->props)
extern int ow_npc_count;
extern int ow_prop_count;
/* 1 if the scratch is the overworld's (ow_scr valid). */
int  ow_scratch_mine(void);
/* Take the scratch for the overworld; returns 1 when it had to be
 * (re)claimed, i.e. citizens, props and images are gone (counts are 0). */
int  ow_scratch_claim(void);
#define ow_bb doomo_frame_bb
DOOMO_STATIC_ASSERT(OW_BB_MAX <= DOOMO_FRAME_BB_MAX, ow_billboards_fit_frame_bb);
extern int ow_bb_count;

/* overworld_npc.c */
void ownpc_load_props(void);
void ownpc_spawn_all(void);
void ownpc_update(void);
int  ownpc_find(int id);
int  ownpc_blocks(int32_t x, int32_t y, int self);    /* NPC at q4 pos?     */
void ownpc_face_player(int i);
void ownpc_add_billboards(int32_t vx0, int32_t vy0, int32_t vx1, int32_t vy1);
int  ownpc_nest_near(int32_t x, int32_t y);             /* nest or -1      */

/* overworld.c */
int  ow_walkable_cell(int cx, int cy, int from_fh);
int  ow_floor_h(int cx, int cy);
int  ow_prop_blocks(int32_t x, int32_t y, int r);

/* overworld_fx.c */
void owfx_scratch_lost(void);              /* images must be made again  */
void owfx_init(void);
void owfx_update(void);
void owfx_draw_world(void);                /* particles, fireworks          */
void owfx_draw_hud(void);
void owfx_draw_pause(void);
const DoomoImage *owfx_marker(int flags, int t);
const DoomoImage *owfx_emote(int emo);
void owfx_minimap(int x, int y, int scale, int map_id);
int  owfx_daylight(void);
void owfx_world_to_screen(int32_t x, int32_t y, int *sx, int *sy);

#endif
