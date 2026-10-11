/* lib_int.h - internals shared by the Grand Library's files (owner:
 * library): lib_common.c (lobby, browser framework, completion, the
 * activities table), one lib_<wing>.c per book and unlocks_library.c.
 * Not part of the contract (that is doomo/library.h).
 *
 * MEMORY: one LibBrowse (the open wing's cursor / tab / detail state) and
 * one LibWingData union (only one wing is open at a time: the lobby pushes
 * a wing, a wing never pushes another) - about 2.3 KB of .bss in total.
 */
#ifndef DOOMO_LIB_INT_H
#define DOOMO_LIB_INT_H

#include "doomo/doomo.h"
#include "doomo/library.h"

/* ---- the browser state of the open wing --------------------------------- */
typedef struct LibBrowse {
    int16_t  count;        /* entries in the current view                   */
    int16_t  sel, top;     /* cursor and first visible row                  */
    int16_t  vis;          /* visible rows (set by the list drawer)         */
    uint8_t  tab;          /* category (L/R)                                */
    uint8_t  filter;       /* SELECT cycle                                  */
    uint8_t  sort;         /* START / SELECT cycle (wing specific)          */
    uint8_t  detail;       /* 1 = the detail page is open                   */
    uint8_t  demo;         /* opened with LIB_ARG_DEMO                      */
    uint8_t  sub;          /* detail sub page (L/R on the detail page)      */
    int8_t   flip;         /* page turn: -1 / +1 while animating            */
    uint8_t  toggle;       /* SELECT toggle on the detail page              */
    uint16_t t;            /* ticks since the wing opened                   */
    uint16_t rest;         /* ticks since the selection last changed        */
    uint16_t dt;           /* ticks since the detail page opened / flipped  */
    uint16_t tab_t;        /* ticks since the tab / filter changed          */
    int16_t  scroll;       /* detail text scroll (lines)                    */
    int16_t  scroll_max;   /* set by the detail drawer                      */
} LibBrowse;
extern LibBrowse lib_b;

/* ---- per-wing scratch (one wing at a time) ------------------------------- */
#define LIB_TROPHY_MAX DOOMO_UNLOCK_BITS
#define LIB_WAVE_COLS  80
typedef union LibWingData {
    struct {                                  /* Citizen Almanac            */
        uint16_t idx[DOOMO_MAX_RESIDENTS];    /* filtered + sorted ids      */
        uint16_t rel_id[6];                   /* relationship summary       */
        int8_t   rel_val[6];
        uint8_t  rel_kind[6];
        uint8_t  rel_n, rel_friends, rel_family, rel_foes;
        int16_t  rel_for;                     /* whose summary that is      */
        uint8_t  letter;                      /* search: 0 = all, 'A'..'Z'  */
        uint8_t  picking;                     /* letter picker open         */
        uint8_t  pick;                        /* letter picker cursor 0..26 */
        uint8_t  expr;                        /* detail portrait expression */
        uint16_t letters;                     /* (unused)                   */
        uint16_t letter_n[27];                /* names per initial (picker) */
    } alm;
    struct {                                  /* Trophy Wall                */
        uint16_t idx[LIB_TROPHY_MAX];         /* unlock_at() indices        */
    } tro;
    struct {                                  /* Food & Item Dex            */
        uint16_t love[FOOD_COUNT], hate[FOOD_COUNT];
        uint8_t  idx[FOOD_COUNT + 16];
        int16_t  fan[FOOD_COUNT];             /* biggest fan of each food   */
    } item;
    struct {                                  /* Weapon Catalog             */
        uint8_t  idx[64];
        DoomoWeapon w, base;                  /* preview + its base weapon  */
        int16_t  w_for;                       /* (list << 8 | index) built  */
        uint8_t  rate[WRATE_COUNT], base_rate[WRATE_COUNT];
    } wpn;
    struct {                                  /* Jukebox                    */
        int8_t   lo[LIB_WAVE_COLS], hi[LIB_WAVE_COLS];
        int16_t  wave_for;                    /* sfx the waveform shows     */
        uint8_t  prev_song, playing, played_n, sfx_n;
        uint32_t played;                      /* songs played this session  */
        uint32_t sfx_played[2];
        uint16_t play_t;                      /* ticks since PLAY           */
        uint8_t  last_sfx;
        uint8_t  spin;
    } juke;
    struct {                                  /* Art Gallery                */
        uint32_t sky_uid[SKY_COUNT];
        uint8_t  rooms;                       /* rooms visited (bits)       */
        uint8_t  paused, frame;
        uint8_t  pal_x, pal_y;
    } gal;
    struct {                                  /* Bestiary                   */
        uint8_t  idx[16];
        uint16_t seen, defeated;              /* bit per demon (on entry)   */
        uint8_t  pct;
    } best;
} LibWingData;
extern LibWingData lib_w;

/* ---- wings ------------------------------------------------------------------ */
typedef struct LibWingInfo {
    const char *title;      /* "BESTIARY"                                   */
    const char *name;       /* "Bestiary"                                   */
    const char *blurb;      /* Quillby's line about the book                */
    uint8_t     icon;       /* ICON_*                                       */
    uint8_t     paint;      /* cover colour (PAINT_*)                       */
    uint8_t     style;      /* WIN_* of the wing's windows                  */
    uint8_t     backdrop;   /* WIN_* of its backdrop                        */
    const DoomoScene *scene;
} LibWingInfo;
extern const LibWingInfo lib_wings[LIB_WING_COUNT];

/* Per-wing completion (each wing's own file). */
int lib_bestiary_pct(void);
int lib_almanac_pct(void);
int lib_weapons_pct(void);
int lib_items_pct(void);
int lib_trophies_pct(void);
int lib_jukebox_pct(void);
int lib_gallery_pct(void);
/* Citizens the player knows (Almanac): count and the test. */
int lib_almanac_known(void);
int lib_known(int id);

/* ---- framework (lib_common.c) ------------------------------------------- */
int  lib_sw(void);
int  lib_sh(void);
int  lib_big(void);                      /* 1 on 320x240-class screens       */
/* Raw unlock bits (no toast). */
int  lib_bit(int id);
void lib_bit_set(int id);
/* Open a wing: reset lib_b / lib_w, mark it visited, play its jingle. */
void lib_wing_enter(int wing, int arg);
/* LIB_ARG_DEMO: premade citizens on an empty island. */
void lib_demo_island(void);
/* Leave the open wing (scene_pop, or back to the title if it is alone). */
void lib_wing_leave(void);

/* Navigation: one call per tick in the wing's update. */
enum { LIBE_NONE, LIBE_MOVED, LIBE_TAB, LIBE_FILTER, LIBE_START, LIBE_OPEN,
       LIBE_BACK, LIBE_FLIP, LIBE_SUB, LIBE_TOGGLE, LIBE_CLOSE, LIBE_SCROLL };
/* tabs = number of categories (L/R in the list), subs = detail sub pages
 * (L/R on the detail page, 0/1 = none). Moves lib_b.sel / tab / sub /
 * scroll itself and returns what happened. */
int  lib_nav(int tabs, int subs);
/* Clamp sel/top after the view changed (count updated). */
void lib_clamp(void);

/* ---- drawing helpers ------------------------------------------------------ */
/* Backdrop + header (title pill with the wing icon, completion badge at the
 * top right). Returns the y below the header. */
int  lib_chrome(int wing, int pct);
/* "< Category (n) >" pill centred at cx (L/R tabs), returns its height. */
int  lib_tab_pill(int cx, int y, const char *name, int n, int tab, int tabs, int style);
/* Hint pill at the bottom. */
void lib_hint(const char *s, int style);
/* Generic list rows: the framework draws the panel, the selection pill,
 * the cursor and the scroll bar; row() draws one entry's contents. */
typedef void (*LibRowFn)(int index, int x, int y, int w, int selected);
void lib_list(int x, int y, int w, int h, int row_h, int style, LibRowFn row);
/* Completion badge "73%" with a mini bar (gold star at 100). */
void lib_badge(int x, int y, int pct, int style);
/* A rubber stamp ("SLAIN", "LOCKED"...) tilted by one pixel per 8 px. */
void lib_stamp(int cx, int cy, const char *s, uint8_t color);
/* Little padlock (7x8). */
void lib_padlock(int x, int y, uint8_t color);
/* Detail page window (boings in, page-turn wipe after LEFT/RIGHT).
 * Returns 1 when the contents should be drawn. */
int  lib_detail_window(int x, int y, int w, int h, int style);
/* Call after drawing the detail page contents (page-turn wipe). */
void lib_detail_end(int x, int y, int w, int h, int style);
/* Text block clipped to `lines` lines starting at lib_b.scroll (sets
 * lib_b.scroll_max); draws scroll arrows at the right edge. */
void lib_text_block(int x, int y, int w, int lines, const char *s, uint8_t color, int style);
/* A labelled stat line "Label ........ value". */
void lib_kv(int x, int y, int w, const char *k, const char *v, uint8_t kc, uint8_t vc);
/* 0..5 threat skulls / stars. */
void lib_pips(int x, int y, int n, int max, int icon, uint8_t on, uint8_t off);
/* Icon glyph on a round coloured chip (radius r). */
void lib_icon_chip(int cx, int cy, int r, int icon, uint8_t fill);
/* Truncate s to fit w pixels (with "."), into buf (>= 40 bytes). */
const char *lib_fit(const char *s, int w, char *buf, int cap);
/* Integer percent n/d (0..100, 0 if d == 0). */
int  lib_pct(int n, int d);

#endif
