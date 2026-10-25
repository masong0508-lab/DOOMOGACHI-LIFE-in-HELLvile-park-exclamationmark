/* show_host_int.h - between the stage-kind host files (owner: show, ENGINE
 * lane 3): show_host.c (the record, keys, events, the filter),
 * show_host_ow.c, show_host_room.c, show_host_v3d.c, show_hud.c and the
 * renderer copies (show_rcopy_*.c).
 */
#ifndef SHOW_HOST_INT_H
#define SHOW_HOST_INT_H

#include "doomo/doomo.h"
#include "doomo/camfx.h"
#include "show/show_hosts.h"
#include "ps1.h"

/* the renderer copies (show_rcopy_*.c): the game's renderers with their
 * frame-to-frame history dropped, so every frame is exactly the keyed one */
void show_ow_render(const DoomoOverworldView *v);
void show_room_render(const DoomoRoomView *v);
void show_v3d_render(const DoomoView3D *v, int view_dist, int sky);

#define SH_MAX_ACTORS 48
#define SH_MAX_NPCS   64
#define SH_MAX_BB     160

/* the parsed record (pointers into it: the record must outlive the shot) */
typedef struct ShowHostCtx {
    const ShowHostHdr   *hdr;
    const ShowHostWorld *world;
    const ShowHostKey   *cam;   int ncam;
    const ShowHostActor *act;   int nact;
    const ShowHostKey   *path;  int npath;
    const ShowHostEvent *ev;    int nev;
    const ShowHostFurn  *furn;  int nfurn;
    const ShowHostNpc   *npc;   int nnpc;
    const char          *str;   uint32_t nstr;
    const ShowHostHud   *hud;
    ShowHostWorld        wdef;  /* used when the record has no SEC_WORLD */
    int kind;
    int ready;
    int t, wt;                  /* shot frame, world time (hit-stops held)   */
    int gore_script;            /* SG_* the script asks for at t              */
    int gore;                   /* SG_* drawn at t (after the kid rule)       */
    int kids;                   /* children in frame at t                     */
    int kid_report;             /* show_gore_kid_in_frame() since the last draw */
    int npc_total;              /* citizens made at prep                      */
} ShowHostCtx;
extern ShowHostCtx g_sh;

/* keys and time (show_host.c) */
void sh_key_eval(const ShowHostKey *k, int n, int t, int32_t out[5]);
int  sh_world_time(int t);
const char *sh_str(int off);

/* an actor at world time wt */
typedef struct ShActor {
    int32_t x, y;               /* 16.16 (world units; ROOM: tiles)          */
    int     z;                  /* world units above the floor               */
    int     dir;                /* SDIR_* resolved                           */
    int     mode;               /* SPM_*                                     */
    int     moving;
    int     frame;              /* walk frame 0..3                           */
    int     visible;
    int     expr;
    int     dead_at, pain_until, attack_until, fall_at;  /* world frames, -1 */
    int     overkill;
} ShActor;
void sh_actor_at(int i, int t, int wt, ShActor *out);

/* the NPC citizens (show_host.c): position of citizen k of group g */
int  sh_npc_count(int g);
int  sh_npc_looks(int g);          /* looks in use (K-look reuse) */
void sh_npc_at(int g, int k, int wt, int32_t *x, int32_t *y, int *dir, int *frame, int *moving);

/* per kind */
int  sh_ow_prep(void);
void sh_ow_draw(int t);
int  sh_room_prep(void);
void sh_room_draw(int t);
int  sh_v3d_prep(void);
void sh_v3d_tick(int t);
void sh_v3d_draw(int t);
void sh_hud_prep(void);
void sh_hud_draw(int t);
void sh_hud_warm(void);
void sh_hud_warm_vram(void);
/* VIEW3D state the HUD reads (show_host_v3d.c) */
int  sh_v3d_flash(int t);           /* muzzle flash frames left at t (0 = none)  */
int  sh_v3d_kick(int t);            /* weapon kick 0..256                         */
void sh_v3d_view(DoomoView3D *out); /* the camera of the last draw                */

/* edges fired by show_host_tick (also from the kinds) */
void sh_set_game_gore(int g);

#endif
