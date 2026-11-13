/* levels_int.h - internals of the levels expansion (owner: levels):
 * Hell Breach 3..7, the level select with ranks, the level play scene and
 * the arena modes. Shared by levels.c (level + mini-boss tables, ranks,
 * records, unlock rules, the shared mini-boss / conveyor / level-script
 * runtime, the activities table), levels_play.c (SCENE_BREACH),
 * levels_select.c (SCENE_LEVEL_SELECT), levels_draw.c (shared 2D pieces)
 * and arena_modes.c (SCENE_ARENA_MODE). Not part of the contract.
 *
 * The shooter itself is the action module's (combat*.c, actor.c); these
 * files drive a combat session the same way SCENE_COMBAT does and add on
 * top of it, through combat_int.h:
 *   - mini-boss VARIANTS: an existing demon type with more HP, a bigger
 *     body and billboard (DoomoBillboard.scale), a glow / ghostly look
 *     (DoomoBillboard.light / BB_TRANSLUCENT), a screen tint while it
 *     fights, its own name on the boss bar, intro taunt, music and a
 *     mechanic (summons, blinking, organ fire, portal spawns);
 *   - conveyor belts: plain cells tagged LV_TAG_CONVEYOR_E..N push the
 *     player (and pickups) along;
 *   - ranks: time vs par, kills and secrets -> S/A/B/C, packed into the
 *     saved PROG_BREACH_RANKS counter (3 bits a level) together with the
 *     best Horde wave; trophies are unlock ids 864..895 (unlock_ids.h).
 * LV0 (levels plan section 9): levels2.c holds the level table and the
 * registry of all 31 levels, levels_rec.c the records of the 24 new ones.
 */
#ifndef DOOMO_LEVELS_INT_H
#define DOOMO_LEVELS_INT_H

#include "combat_int.h"
#include "doomo/unlock_ids.h"

/* ---- the levels ------------------------------------------------------------ */
/* Ids 0..6 are the seven built levels (select page 1). LV0 (levels plan
 * 2.5) registers 24 more: Hell Breach 8-16 are ids 7..15 (page 2), the
 * New Game+ "Arrears" Notices 1-15 are ids 16..30 (page 3). An id never
 * changes: records are saved by it. */
enum { LV_VAPE_CELLAR, LV_CLOUD_NINE, LV_SEWERS, LV_FACTORY, LV_GRAVEYARD,
       LV_CATHEDRAL, LV_UNDERBELLY,
       LV_HB8, LV_HB9, LV_HB10, LV_HB11, LV_HB12, LV_HB13, LV_HB14, LV_HB15, LV_HB16,
       LV_N1, LV_N2, LV_N3, LV_N4, LV_N5, LV_N6, LV_N7, LV_N8, LV_N9, LV_N10,
       LV_N11, LV_N12, LV_N13, LV_N14, LV_N15,
       LV_COUNT };
#define LV_FIRST_NEW LV_SEWERS
#define LV_OLD_COUNT 7                /* ranks in PROG_BREACH_RANKS, golds as unlock ids */
#define LV2_COUNT    (LV_COUNT - LV_OLD_COUNT)   /* records in the 'BRCH' chunk (levels_rec.c) */
#define LV_NO_MAP    255              /* LevelInfo.map of a level that is not built yet */
/* Rows the tables hold on this build. The PS1 keeps the old seven only until
 * its resident-memory share is released (PARALLEL_WAVES.md section 3: no new
 * resident byte before PS1-RAM): its tables, 'BRCH' and the new records are
 * left out and every new level reads as "not listed". Flip it there. */
#if defined(DOOMO_PLATFORM_PS1)
#define LV_HAS_NEW   0                /* (a number for #if: the enum constants are 0 there) */
#define LV_TABLE_N   LV_OLD_COUNT
#else
#define LV_HAS_NEW   1
#define LV_TABLE_N   LV_COUNT
#endif
DOOMO_STATIC_ASSERT(LV_OLD_COUNT == LV_HB8 && LV_N1 == 16 && LV_COUNT == 31, level_ids_are_saved);

typedef struct LevelInfo {
    const char *title;        /* "Sugar Rush Sewers"                      */
    const char *tag;          /* "HELL BREACH 3"                          */
    const char *blurb;        /* two short lines for the select screen    */
    const char *flavor;       /* intro card tagline                       */
    uint8_t  map;             /* MAPID_*                                  */
    uint8_t  boss;            /* LB_* mini-boss, or LB_NONE               */
    uint16_t par;             /* par time, seconds (A/S ranks need it)    */
    uint16_t dev;             /* the developers' time trial (gold medal)  */
    uint8_t  hell_song;       /* music once the level turns hellish        */
    uint8_t  theme;           /* WIN_* style of its select card           */
    uint8_t  accent;          /* palette index for its card               */
    uint8_t  icon;            /* ICON_*                                   */
    uint8_t  pv_x, pv_y;      /* preview camera cell (select screen)      */
    uint8_t  pv_z;            /* preview eye height above the floor (8u)  */
    uint8_t  hell_at;         /* hell blend (0..255) that switches music   */
} LevelInfo;
extern const LevelInfo lv_info[LV_TABLE_N];       /* levels2.c */

/* ---- the registry (levels2.c): page, platform manifest, unlock rule ---------- */
enum { LVP_BREACH, LVP_HELL, LVP_ARREARS, LVP_COUNT };   /* select pages */
/* LevelReg.flags */
enum { LVG_GBA = 1,         /* on the GBA cartridge (plan 7.1)                 */
       LVG_DARK = 2 };      /* a dark level (plan 6; LV11 shows its note)      */
/* Unlock rule kinds. A level of page LVP_ARREARS needs New Game+ on top of
 * its own rule; a level that was cleared once stays open. */
enum { LVK_ALWAYS,
       LVK_AFTER_LEVEL,     /* arg = LV_*: that level cleared                  */
       LVK_CHAPTER,         /* arg = STORY_CH*: story_chapter() >= arg         */
       LVK_BEAT,            /* arg = BEAT_EP*: that episode done (or a later chapter) */
       LVK_FLAG,            /* arg = SF_*: the story flag is set               */
       LVK_NGPLUS,          /* SF_NGPLUS, or the Arrears arc was finished      */
       LVK_COUNT };
/* Text ids of the locked reason (lv_reason_text[]). */
enum { LVT_NONE, LVT_CLEAR_FIRST, LVT_CH2, LVT_CH3, LVT_CH4, LVT_POSTGAME, LVT_NGPLUS,
       LVT_NG_CH1, LVT_NG_CH2, LVT_NG_CH3, LVT_NG_CH4, LVT_NOTICE_BEFORE, LVT_NOT_BUILT,
       LVT_COUNT };
typedef struct LevelReg {
    uint8_t page;             /* LVP_*                                    */
    uint8_t flags;            /* LVG_*                                    */
    uint8_t rule;             /* LVK_*                                    */
    uint8_t arg;              /* the rule's level / chapter / story flag  */
    uint8_t reason;           /* LVT_*: why it is locked, for the select  */
} LevelReg;
extern const LevelReg lv_reg[LV_TABLE_N];
extern const char *const lv_reason_text[];
extern const char *const lv_page_names[LVP_COUNT];
/* The platform manifest: the GBA lists the 7 old levels and its seven of
 * the new ones (LVG_GBA), every other platform all 31 (the PS1: see
 * LV_TABLE_N). The host build can pretend to be the GBA for the tests:
 * lv_list_req = LVG_GBA (a level is listed when it has every flag asked). */
#if defined(DOOMO_PLATFORM_GBA) && !defined(DOOMO_LEVELS_GBA_SET)
#define DOOMO_LEVELS_GBA_SET 1
#endif
#if defined(DOOMO_LEVELS_GBA_SET)
#define LV_REC_N 7                    /* new levels with a record on this build */
#define LV_LIST_REQ LVG_GBA
#elif defined(DOOMO_PLATFORM_PS1)
#define LV_REC_N 0
#define LV_LIST_REQ 0
#else
#define LV_REC_N LV2_COUNT
extern uint8_t lv_list_req;           /* tests only */
#define LV_LIST_REQ lv_list_req
#endif
int  lv_listed(int level);            /* 1 if this build has the level at all   */
int  lv_listed_count(void);
int  lv_rec_slot(int level);          /* index of a new level's record, or -1   */
int  lv_built(int level);             /* listed and it has a map                */
int  lv_page_of(int level);           /* LVP_* (0 for a bad id)                 */

/* ---- mini-boss variants ------------------------------------------------------ */
enum { LB_GUTTERKING, LB_FLUFFNUTS, LB_WIDOW, LB_BONEBOTTOM, LB_GRUMBLEGUTS,
       LB_CELLAR_WRAITH, LB_VAPULA, LB_COUNT, LB_NONE = 255 };
/* LevelBoss.flags */
enum { LBF_GHOST = 1,       /* translucent flicker                            */
       LBF_SUMMON = 2,      /* calls `add` demons in vape puffs               */
       LBF_BLINK = 4,       /* teleports around the arena                     */
       LBF_ORGAN = 8,       /* the pipe organ shoots fire while it lives      */
       LBF_PORTALS = 16,    /* imps pour out of the portals while it lives    */
       LBF_PULSE = 32 };    /* throbbing glow                                 */

typedef struct LevelBoss {
    const char *name;         /* boss bar + intro                          */
    const char *title;        /* "the Sticky Sovereign of the Sewers"       */
    const char *taunt;        /* intro line                                 */
    const char *death;        /* toast when it falls                        */
    uint8_t  type;            /* THING_* base demon                         */
    uint16_t scale;           /* billboard size, 8.8 (256 = normal)          */
    uint8_t  hp8;             /* HP vs a regular map boss, eighths           */
    uint8_t  glow;            /* extra light 0..15                          */
    uint8_t  flags;           /* LBF_*                                      */
    uint8_t  add;             /* THING_* summoned / portal demon            */
    uint8_t  tint, tint_lvl;  /* screen tint while it fights                */
    uint8_t  bar;             /* boss bar colour                            */
    uint8_t  spr;             /* SPR_* for portraits in menus               */
} LevelBoss;
extern const LevelBoss lv_boss[LB_COUNT];

/* ---- runtime: one shared state for the session (levels.c) ------------------- */
#define LV_TRACK 6                    /* variant demons tracked at once     */
typedef struct LvTrack {
    int8_t   actor;                   /* doomo_actors index, -1 = free      */
    uint8_t  boss;                    /* LB_*                               */
    uint8_t  woke;                    /* intro shown                        */
    uint8_t  dead;
    int16_t  hp_max;
    uint16_t t;                       /* mechanic timer                     */
} LvTrack;

/* Per-actor size class for Monster Mode Mayhem (0 = normal). */
enum { LVS_NONE, LVS_TINY, LVS_BIG, LVS_HUGE };

typedef struct LvRun {
    LvTrack  track[LV_TRACK];
    uint8_t  size_cls[ACTOR_MAX / 4]; /* LVS_* (mayhem), 2 bits an actor: lv_size_cls() (LV0: 128 B -> 32 B) */
    int8_t   bar;                     /* track index shown on the boss bar  */
    uint8_t  intro_boss;              /* LB_* of the running intro banner   */
    uint16_t intro_t;                 /* ticks left of the intro banner     */
    uint16_t music_hell;              /* hell music switched                */
    uint8_t  level;                   /* LV_* or 255 (arena)                */
    uint8_t  in_boss;                 /* boss music on                      */
    uint8_t  organ_n;                 /* organ pipe sources                 */
    uint8_t  organ_x[6], organ_y[6];
    uint8_t  portal_n;
    uint8_t  portal_x[6], portal_y[6];
    uint16_t base_music;
} LvRun;
extern LvRun lv_run;

/* Start the shared runtime after combat_start*(): level = LV_* or 255. */
void lv_runtime_begin(int level);
/* Make actor `a` a mini-boss variant (track it; HP, size, glow). Returns
 * the track slot or -1. */
int  lv_make_boss(int a, int boss, int hp_scale8);
/* Spawn a variant at a cell (arena modes). Returns the actor index or -1. */
int  lv_spawn_boss(int boss, int cx, int cy, int hp_scale8);
/* One tick of the runtime BEFORE combat_tick (conveyors) and AFTER it
 * (boss wake banners, mechanics, deaths, music). */
void lv_runtime_pre(void);
void lv_runtime_post(void);
/* Scale billboards of variant demons in a built view + their screen tint. */
void lv_runtime_view(DoomoView3D *v);
/* HUD with our boss bar instead of the stock one (and the intro banner). */
void lv_runtime_hud(void);
/* 1 if any tracked variant is alive (and awake if `awake`). */
int  lv_bosses_alive(int awake);
/* Make a demon tiny / big / huge (mayhem). */
void lv_size_demon(int a, int cls);
/* The size class of actor `a` (LVS_*), and setting it without touching the actor. */
#define lv_size_cls(a) ((lv_run.size_cls[(a) >> 2] >> (((a) & 3) * 2)) & 3)
void lv_size_cls_set(int a, int cls);

/* Conveyor tags (plain cells, no flags): push east / south / west / north. */
#define LV_TAG_CONVEYOR_E 240
#define LV_TAG_CONVEYOR_N 243

/* ---- ranks & records (levels.c) ---------------------------------------------- */
enum { RANK_NONE, RANK_C, RANK_B, RANK_A, RANK_S };
extern const char *const lv_rank_names[5];       /* "-", "C", "B", "A", "S" */
typedef struct LvScore {
    int time_pts, kill_pts, secret_pts, total;   /* 0..40, 0..35, 0..25 */
    int rank;                                    /* RANK_*               */
} LvScore;
void lv_score(int level, const DoomoCombatStats *s, LvScore *out);
int  lv_rank(int level);                         /* best rank, RANK_*     */
int  lv_set_rank(int level, int rank);           /* 1 if improved         */
int  lv_unlocked(int level);                     /* playable in the select */
int  lv_prereq_of(int level);                    /* level to clear first, -1 */
/* LV0: one rule of the table (LVK_*, its arg), and the rule of `level` with
 * the page's New Game+ condition. */
int  lv_rule_eval(int kind, int arg);
int  lv_rule_met(int level);
/* The level to start for a SCENE_BREACH / select argument: `level` itself
 * when it is built, else the last built one of the old seven (what the
 * clamp did before LV0). */
int  lv_startable(int level);
/* Why `level` is locked, as a line for the select ("" when it is open). */
void lv_locked_reason(int level, char *buf, int cap);
int  lv_cleared_count(void);
int  lv_gold(int level);                         /* time trial gold medal */
int  lv_horde_best(void);
void lv_set_horde_best(int wave);
uint8_t lv_rank_color(int rank);
/* Session results (after a clear): grants ranks, trophies, rewards. Fills
 * *score; returns a bitmask of LVR_* news for the results screen. */
enum { LVR_NEW_RANK = 1, LVR_FIRST_CLEAR = 2, LVR_GOLD = 4, LVR_CHALLENGE = 8 };
int  lv_finish(int level, int mode, const DoomoCombatStats *s, LvScore *score, int *bonus);
/* Grant the Monster Mode Mayhem gate if Monster Mode is on. */
void lv_check_monster(void);

/* ---- records of the new levels + the Arrears arc state (levels_rec.c) ----------
 * The 'BRCH' save chunk (save_ext.h): 4 B per new level (rank 3 bits, gold
 * 1, feats 3, cleared 1; secrets 8; collectibles 16) and 48 B of arc
 * state. The old seven stay in PROG_BREACH_RANKS and their gold unlock ids;
 * lv_rank() / lv_set_rank() / lv_gold() answer for all 31. Setters only
 * ever add (a better rank, more secrets, more bits); a level this build
 * does not list records nothing. */
#define LV_FEAT_MASK 7
#if LV_HAS_NEW
int      lv_rec_rank(int level);
int      lv_rec_set_rank(int level, int rank);       /* 1 if improved   */
int      lv_rec_gold(int level);
void     lv_rec_set_gold(int level);
int      lv_rec_cleared(int level);
void     lv_rec_set_cleared(int level);
int      lv_arc_finished(void);                      /* act == LVA_DONE */
#else   /* the PS1 for now: no records, no arc (LV_TABLE_N) */
#define lv_rec_rank(l)        RANK_NONE
#define lv_rec_set_rank(l, r) 0
#define lv_rec_gold(l)        0
#define lv_rec_set_gold(l)    ((void)0)
#define lv_rec_cleared(l)     0
#define lv_rec_set_cleared(l) ((void)0)
#define lv_arc_finished()     0
#endif
int      lv_rec_feats(int level);                    /* 3 bits          */
void     lv_rec_add_feats(int level, int mask);
int      lv_rec_secrets(int level);                  /* best, 0..255    */
void     lv_rec_set_secrets(int level, int n);
unsigned lv_rec_collect(int level);                  /* 16 bits         */
void     lv_rec_add_collect(int level, unsigned mask);
/* the arc: nothing reads these before LV22 */
enum { LVA_NONE, LVA_COLD_OPEN, LVA_ACT1, LVA_ACT2, LVA_MIDPOINT, LVA_ACT3, LVA_FINALE,
       LVA_DONE };                                   /* lv_arc_act()    */
#define LV_ARC_FLAGS 16
#define LV_ARC_HELD  16
#define LV_ARC_NOBODY 0xFFFFu
int      lv_arc_act(void);
void     lv_arc_set_act(int act);
int      lv_arc_flag(int i);                         /* 0..15           */
void     lv_arc_set_flag(int i, int on);
unsigned lv_arc_buyback(void);                       /* 16-bit mask     */
void     lv_arc_set_buyback(unsigned mask);
int      lv_arc_choice(int i);                       /* i 0..1, a byte  */
void     lv_arc_set_choice(int i, int v);
int      lv_arc_held_count(void);
unsigned lv_arc_held(int i);                         /* resident id, LV_ARC_NOBODY */
int      lv_arc_hold(unsigned resident);             /* 1 held (or already), 0 full */
void     lv_arc_release(unsigned resident);
int      lv_arc_last_ending(void);                   /* 255 = none      */
void     lv_arc_set_last_ending(int ending);
int      lv_arc_line4(void);                         /* the fourth player line */
void     lv_arc_set_line4(int v);
/* 'BRCH' sizes: header + arc + the records this build lists. */
#define LV_BRCH_VERSION 1
#define LV_BRCH_HDR     4
#define LV_BRCH_ARC     48
#define LV_BRCH_MAX     (LV_BRCH_HDR + LV_BRCH_ARC + 4 * LV2_COUNT)      /* 148 B */

/* ---- play modes (SCENE_BREACH arg = level | mode << 8 | LVF_*) ----------------- */
enum { LVM_NORMAL, LVM_TIME_TRIAL, LVM_CUTE_ONLY, LVM_BRUTAL_ONLY, LVM_COUNT };
#define LVF_DEMO   0x1000             /* the autopilot plays (attract)        */
#define LV_ARG(level, mode) ((level) | ((mode) << 8))
extern const char *const lv_mode_names[LVM_COUNT];

/* ---- unlock ids (864..895, unlock_ids.h) ---------------------------------------- */
#define LVU_GOLD0        864          /* +level: time trial gold (7)        */
#define LVU_TIME_TRIAL   871          /* gate: Time Trial                    */
#define LVU_CHALLENGES   872          /* gate: Cute-Only / Brutal-Only       */
#define LVU_CUTE_CLEAR   873
#define LVU_BRUTAL_CLEAR 874
#define LVU_BOSS_RUSH    875
#define LVU_BOSS_RUSH_FAST 876
#define LVU_HORDE10      877
#define LVU_HORDE20      878
#define LVU_HORDE30      879          /* reward: flame wings (creator part alias) */
#define LVU_MAYHEM       880          /* gate: Monster Mode Mayhem           */
#define LVU_MAYHEM100    881
#define LVU_ALL_CLEAR    882
#define LVU_FIRST_S      883
#define LVU_BOSS0        884          /* +LB_*: mini-boss defeated (5)      */
#define LVU_ALL_S        889          /* reward: crown (creator part alias) */
#define LVU_ALL_GOLD     890          /* reward: rollerskates alias          */
#define LVU_LAST         890

/* ---- shared 2D bits (levels_draw.c) ------------------------------------------- */
int  lvd_big(void);                              /* 320x240 layouts        */
/* Rank letter in a stamp; t = ticks since it appeared (slam-in). */
void lvd_rank_stamp(int cx, int cy, int rank, int t, int big);
/* "1:23" */
void lvd_time(char *buf, int cap, int seconds);
/* Animated title banner with a ribbon and sparkles. */
void lvd_banner(int cy, const char *kicker, const char *title, int t, int hell);
void lvd_banner_band(int cy, int *y, int *h);   /* (S2-2) */
/* A demon's portrait (its walk frame) at x,y (feet at y), bobbing. */
void lvd_demon(int x, int y, int spr, int t, int scale, int flip);
/* A wobbly toy padlock (locked levels), k = pixel scale 1..3. */
void lvd_padlock(int cx, int cy, int k, int t);
/* Mini-boss intro card over the 3D view. */
void lvd_boss_intro(int boss, int t);
/* the boss's own picture (Grumbleguts as the concept figure, feet at (x, y);
 * the others via lvd_demon) */
void lvd_boss_figure(int boss, int x, int y, int t, int scale, int flip);
/* Top of our boss bar (its name sits 10 px above); HUD bits that go
 * under it start at lvd_bar_y() + 11. */
int  lvd_bar_y(void);
/* Our boss bar (variant name + HP). */
void lvd_boss_bar(int boss, int hp, int hp_max, int t);

/* ---- arena modes (arena_modes.c) --------------------------------------------- */
enum { AM_BOSS_RUSH, AM_HORDE, AM_MAYHEM, AM_COUNT };

#endif
