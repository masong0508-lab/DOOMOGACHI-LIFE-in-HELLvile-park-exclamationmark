/* creator_int.h - private glue between creator.c (scene logic),
 * creator_draw.c (all drawing) and creator_data.c (option tables, presets,
 * texts, unlocks) (owner: creator). Not part of the public contract; the
 * public bits are SCENE_CREATOR, creator_open() and creator_last_result()
 * in scenes.h. See docs/CREATOR.md.
 */
#ifndef DOOMO_CREATOR_INT_H
#define DOOMO_CREATOR_INT_H

#include "doomo/doomo.h"

/* ---- tabs ------------------------------------------------------------- */
enum { CR_TAB_BODY, CR_TAB_FACE, CR_TAB_HAIR, CR_TAB_SPOOKY, CR_TAB_OUTFIT,
       CR_TAB_VOICE, CR_TAB_PERSONA, CR_TAB_PROFILE, CR_TAB_HOME,
       CR_TAB_COUNT };

/* ---- rows ---------------------------------------------------------------
 * Every row edits one byte of the draft DoomoResident at `field` (an
 * offsetof). The kinds: */
enum {
    RK_ENUM,        /* value = base + index, index < count; names[index] or a
                       number; clist = CLIST_* unlock list or CR_NOLIST      */
    RK_SLIDER,      /* 0..count-1; names = { low end, high end } labels      */
    RK_PAINT,       /* paint colour 0..31 (paint picker)                     */
    RK_PAINT_NONE,  /* paint or LOOK_NO_COLOR (hair highlight)               */
    RK_SKIN,        /* look.skin: 0..7 natural tones, 8..39 paints           */
    RK_FLAG,        /* look.cheeks bit `aux`, On/Off                         */
    RK_RFLAG,       /* DoomoResident.flags bit `aux`, Yes/No                 */
    RK_NAME,        /* the name: A types it, LEFT/RIGHT rolls a name idea    */
    RK_ACTION,      /* aux = CA_*: LEFT/RIGHT picks, A runs                  */
    RK_DISTRICT     /* new citizens: which side of the park they move to     */
};
#define CR_NOLIST 0xFF
#define CR_NOFIELD 0xFF

/* actions (RK_ACTION aux) */
enum { CA_SPECIES_PRESET, CA_OUTFIT_PRESET, CA_VOICE_PRESET, CA_TEST_VOICE };

typedef struct CrRow {
    uint8_t kind, field, count, base, clist, aux;
    const char *label;
    const char *const *names;
} CrRow;

typedef struct CrTab {
    const char *name;           /* "Head & Face"                         */
    const CrRow *rows;
    uint8_t nrows;
    uint8_t icon;               /* index into cr_icons                   */
    uint8_t paint;              /* PAINT_* accent of the tab icon        */
    uint8_t rand_look;          /* 1: "randomise tab" rolls a look       */
} CrTab;

extern const CrTab cr_tabs[CR_TAB_COUNT];

/* ---- presets: lists of (field, value) patches ending at CR_NOFIELD ------- */
#define CR_PRESET_KV 34
typedef struct CrPreset {
    const char *name;
    uint8_t kv[CR_PRESET_KV];
} CrPreset;
extern const CrPreset cr_species_presets[];     /* 3 per species, in order */
extern const int cr_species_preset_count;
#define CR_PRESETS_PER_SPECIES 3
extern const CrPreset cr_outfit_presets[];
extern const int cr_outfit_preset_count;
extern const CrPreset cr_voice_presets[];
extern const int cr_voice_preset_count;
/* new-citizen templates: Hillvile cutie, Hellvile cutie */
extern const CrPreset cr_template_hillvile, cr_template_hellvile;

/* ---- texts --------------------------------------------------------------- */
extern const char *const cr_persona_desc[PERSONA_TYPE_COUNT];
extern const uint8_t cr_persona_expr[PERSONA_TYPE_COUNT];
extern const char *const cr_quirk_names[4];
extern const char *const cr_onoff_names[2];
extern const char *const cr_yesno_names[2];
extern const char *const cr_district_names[2];
extern const char *const cr_home_names[2];      /* "Hillvile Towers" ...    */

/* ---- tab icons: 9x9, chars '.' clear, 'o' outline, 's' shade, 'b' base,
 * 'l' light, 'w' white ---------------------------------------------------- */
#define CR_ICON_W 9
extern const char *const cr_icons[CR_TAB_COUNT][CR_ICON_W];

/* ---- the editor state ------------------------------------------------------ */
#define CR_UNDO   20         /* history snapshots: 19 undo steps            */
#define CR_SPARKS 12

enum { CR_NEW, CR_EDIT, CR_PLAYER };
enum { CRP_MAIN, CRP_PICK, CRP_COLOR, CRP_MENU, CRP_COPY, CRP_PREVIEW,
       CRP_MOVEIN };
/* START menu entries */
enum { CM_DONE, CM_RANDOM_ALL, CM_RANDOM_TAB, CM_UNDO, CM_REDO, CM_COPY,
       CM_SPOOKY, CM_RESET, CM_QUIT, CM_COUNT };

typedef struct CrSpark {
    int16_t x, y;           /* 1/16 px, relative to the figure's feet       */
    int8_t  vx, vy;
    uint8_t life, kind;     /* kind 0 sparkle, 1 heart, 2 star              */
} CrSpark;

typedef struct CrState {
    DoomoResident draft;            /* what the player is making             */
    DoomoResident orig;             /* as it was when the creator opened     */
    DoomoResident hist[CR_UNDO];    /* timeline ring; hist[hcur] == draft    */
    uint8_t  hbase, hcount, hcur;
    int16_t  last_key;              /* row that made the last commit         */
    uint32_t last_commit_t;

    int16_t  arg;                   /* scene arg                             */
    int16_t  target;                /* id edited / fixed slot, -1 = alloc    */
    uint8_t  mode;                  /* CR_NEW / CR_EDIT / CR_PLAYER          */
    uint8_t  district;              /* 0 Hillvile, 1 Hellvile                */
    uint8_t  district_fixed;        /* district can't be changed             */
    uint8_t  name_typed;            /* the player typed a name               */

    uint8_t  tab, phase;
    uint8_t  row[CR_TAB_COUNT], top[CR_TAB_COUNT];
    int16_t  browse;                /* locked value being browsed, or -1     */
    uint16_t tab_t, phase_t;        /* ticks since the tab / phase changed   */
    uint32_t t;                     /* ticks since the creator opened        */

    /* pickers */
    int16_t  pick_sel, pick_top, pick_orig;
    uint8_t  menu_sel;
    uint8_t  copy_src;              /* 0 town, 1 premade roster              */
    int16_t  copy_sel, copy_top, copy_count;

    /* preview */
    uint8_t  spooky;                /* possessed preview                     */
    uint8_t  view8, walking, spin;  /* turntable                              */
    uint8_t  hop;                   /* ticks left of the change hop          */
    uint8_t  expr_i;
    uint16_t expr_t;
    DoomoLook shown;                /* look on screen (throttled)            */
    uint8_t  shown_locked;          /* ... is a locked option: silhouette     */
    uint32_t shown_t;
    DoomoLook face;                 /* look of the portrait medallion        */
    uint8_t  face_locked;
    CrSpark  spark[CR_SPARKS];
    uint8_t  fbuf[FIGHTER_W * FIGHTER_H];
    DoomoImage fimg;
    uint32_t fkey;

    /* voice test */
    const char *talk;
    uint8_t  talk_pos, talk_len;
    uint16_t talk_t;

    /* name entry, finishing, results */
    char     namebuf[DOOMO_NAME_LEN];
    uint8_t  finishing;
    int8_t   preset_i, outfit_i, voice_i;
    int16_t  home_id;               /* slot a new citizen would move into    */
    uint16_t free_homes[2];
    int16_t  result;                /* id written, -1 cancelled              */
    char     msg[48];               /* move-in caption                       */
} CrState;

extern CrState cr;

/* ---- creator.c helpers used by the drawing code ---------------------------- */
const CrRow *cr_cur_row(void);
const CrRow *cr_row_at(int tab, int i);
int  cr_row_value(const DoomoResident *r, const CrRow *row);  /* index form */
int  cr_row_count(const CrRow *row);                           /* choices    */
int  cr_value_locked(const CrRow *row, int index);
/* Text of choice `index` of a row (buf >= 48 bytes). */
const char *cr_choice_name(const CrRow *row, int index, char *buf);
/* Unlock hint for a locked choice ("Win 3 exorcisms"), "" if open. */
const char *cr_lock_hint(const CrRow *row, int index);
int  cr_preset_locked(const CrPreset *p, const char **hint);
int  cr_undo_count(void);
int  cr_redo_count(void);
int  cr_visible_rows(void);
int  cr_list_rows_y(void);
int  cr_is_hellish(void);              /* Hellvile flavour for the visuals */
const char *cr_species_preset_name(int i);
/* Apartment floor (1..50) and room (1..10) of a resident slot. */
void cr_home_floor_room(int id, int *floor, int *room);
const char *cr_menu_label(int i, char *buf);
int  cr_menu_enabled(int i);
/* copy picker: k-th entry's resident id / record */
int  cr_copy_id_at(int k);
const DoomoResident *cr_copy_res(int id);

/* ---- creator_draw.c ------------------------------------------------------- */
void cr_draw(void);
void cr_layout(void);                   /* recompute rects for the screen  */
/* screen rects (filled by cr_layout) */
typedef struct CrLayout {
    int sw, sh, big;                    /* big = 320x240-class screen      */
    int px, py, pw, ph;                 /* preview pane                    */
    int lx, ly, lw, lh;                 /* list window                     */
    int tx, ty;                         /* tab strip                       */
    int rows;                           /* visible list rows               */
    int fig_scale, ftr_scale;           /* figure / turntable scale        */
} CrLayout;
extern CrLayout crl;

#endif
