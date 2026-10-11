/* creator_int.h - private glue between creator.c (scene logic, editing,
 * undo, pickers, writing into the town), creator_draw.c (all drawing) and
 * creator_data.c (row tables, presets, texts, icons, unlockables)
 * (owner: creator). Not part of the public contract: the public bits are
 * SCENE_CREATOR, creator_last_result() and creator_nickname() (scenes.h).
 * See docs/CREATOR.md.
 */
#ifndef DOOMO_CREATOR_INT_H
#define DOOMO_CREATOR_INT_H

#include "doomo/doomo.h"

/* ---- tabs ------------------------------------------------------------- */
enum { CR_TAB_BODY, CR_TAB_FACE, CR_TAB_HAIR, CR_TAB_SPOOKY, CR_TAB_OUTFIT,
       CR_TAB_VOICE, CR_TAB_PERSONA, CR_TAB_PROFILE, CR_TAB_HOME,
       CR_TAB_COUNT };

/* ---- rows ---------------------------------------------------------------
 * Every editable row edits one byte of the draft DoomoResident at `field`
 * (an offsetof), so "randomise this tab", "copy", undo and the tests can
 * treat rows generically. Kinds: */
enum {
    RK_HEADER,      /* section caption, never selected                       */
    RK_ENUM,        /* value = base + index, index < count; names[index] (or
                       a number when names == NULL); clist = CLIST_* unlock
                       list or CR_NOLIST                                      */
    RK_SLIDER,      /* 0..count-1; names = { low end, high end }             */
    RK_PAINT,       /* paint colour 0..31 (swatch picker)                    */
    RK_PAINT_NONE,  /* index 0 = LOOK_NO_COLOR, 1..32 = paint (hair highlight) */
    RK_SKIN,        /* look.skin: 0..7 natural tones, 8..39 paints           */
    RK_FLAG,        /* bit `aux` of the byte (look.cheeks): Off / On          */
    RK_RFLAG,       /* bit `aux` of DoomoResident.flags; names = {off, on}   */
    RK_NAME,        /* the name: A types it, LEFT/RIGHT rolls a name idea    */
    RK_NICK,        /* the nickname (creator_nickname), A types it           */
    RK_ACTION,      /* aux = CA_*: LEFT/RIGHT picks a choice, A runs it      */
    RK_DISTRICT     /* new citizens: which side of the park they move to     */
};
#define CR_NOLIST  0xFF
#define CR_NOFIELD 0xFF

/* actions (RK_ACTION aux) */
enum { CA_SPECIES_PRESET, CA_OUTFIT_PRESET, CA_VOICE_PRESET, CA_TEST_VOICE,
       CA_MATCH_FUR, CA_COUNT };

typedef struct CrRow {
    uint8_t kind, field, count, base, clist, aux;
    const char *label;
    const char *const *names;
} CrRow;

typedef struct CrTab {
    const char *name;           /* "Head & Face"                         */
    const char *shortname;      /* "Face"                                */
    const CrRow *rows;
    uint8_t nrows;
    uint8_t paint;              /* PAINT_* accent of the tab icon        */
} CrTab;

extern const CrTab cr_tabs[CR_TAB_COUNT];

/* ---- presets: (field, value) patches ending at CR_NOFIELD ----------------- */
#define CR_PRESET_KV 40
typedef struct CrPreset {
    const char *name;
    uint8_t kv[CR_PRESET_KV];
} CrPreset;
#define CR_PRESETS_PER_SPECIES 3
extern const CrPreset cr_species_presets[SPECIES_COUNT * CR_PRESETS_PER_SPECIES];
extern const CrPreset cr_outfit_presets[];
extern const int cr_outfit_preset_count;
extern const CrPreset cr_voice_presets[];
extern const int cr_voice_preset_count;
/* Look fields that make a species what it is (skin, ears, tail, nose ...):
 * changing the species resets them to `neutral` and then applies the
 * species' first preset to them. Terminated by CR_NOFIELD. */
typedef struct CrFieldDef { uint8_t field, neutral; } CrFieldDef;
extern const CrFieldDef cr_species_base[];

/* ---- texts --------------------------------------------------------------- */
extern const char *const cr_persona_desc[PERSONA_TYPE_COUNT];
extern const char *const cr_persona_greet[PERSONA_TYPE_COUNT];
extern const uint8_t cr_persona_expr[PERSONA_TYPE_COUNT];
extern const char *const cr_quirk_names[4];
extern const char *const cr_district_names[2];   /* "Hillvile" "Hellvile"  */
extern const char *const cr_home_names[2];       /* "Hillvile Towers" ...  */
extern const char *const cr_move_in_lines[8];    /* "%s moved in!" flavour */

/* ---- tab icons: 11x11, '.' clear, 'o' outline, 's' shade, 'b' base,
 * 'l' light, 'w' white, 'p' pink accent ------------------------------------ */
#define CR_ICON_W 11
extern const char *const cr_icons[CR_TAB_COUNT][CR_ICON_W];

/* ---- creator unlockables: ids 256..319 (unlocks.h) ------------------------ */
enum { CRU_MOVE_TO_HELLVILE = 270, CRU_MAKEOVER, CRU_SELF_PORTRAIT,
       CRU_UNDO_LOTS, CRU_VOICE_TESTS, CRU_RANDOMISE_LOTS, CRU_COPY_PREMADE,
       CRU_POSSESSED_PEEK };

/* ---- the editor state ------------------------------------------------------ */
#define CR_UNDO   20         /* history snapshots: 19 undo steps            */
#define CR_SPARKS 14
#define CR_NICKS  64         /* nickname table entries                      */

enum { CR_NEW, CR_EDIT, CR_PLAYER };
enum { CRP_MAIN, CRP_PICK, CRP_COLOR, CRP_MENU, CRP_COPY, CRP_TURN,
       CRP_MOVEIN };
/* START menu entries */
enum { CM_DONE, CM_RANDOM_ALL, CM_RANDOM_TAB, CM_UNDO, CM_REDO, CM_COPY,
       CM_SPOOKY, CM_RESET, CM_QUIT, CM_COUNT };

typedef struct CrSpark {
    int16_t x, y;           /* 1/16 px, relative to the preview's feet      */
    int8_t  vx, vy;
    uint8_t life, kind;     /* kind 0 sparkle, 1 heart, 2 star, 3 confetti  */
    uint8_t color;
} CrSpark;

/* what the shared pixel buffer currently holds */
enum { CRB_NONE, CRB_PORTRAIT, CRB_FIGHTER };

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
    uint8_t  name_ok;               /* the player typed / confirmed a name   */

    uint8_t  tab, phase;
    uint8_t  row[CR_TAB_COUNT], top[CR_TAB_COUNT];
    int16_t  browse;                /* locked choice being looked at, or -1  */
    uint16_t tab_t, phase_t;        /* ticks since the tab / phase changed   */
    uint32_t t;                     /* ticks since the creator opened        */

    /* pickers */
    int16_t  pick_sel, pick_top, pick_orig;
    uint8_t  pick_err;              /* ticks of "locked!" shake              */
    uint8_t  menu_sel;
    uint8_t  copy_src;              /* 0 town, 1 premade roster              */
    int16_t  copy_sel, copy_top, copy_count;
    int16_t  copy_memo_k, copy_memo_id;

    /* preview */
    uint8_t  spooky;                /* possessed preview                     */
    uint8_t  view8, walking, spin;  /* turntable                              */
    uint8_t  roll;                  /* ticks left of a turntable tumble      */
    uint8_t  hop;                   /* ticks left of the change hop          */
    uint8_t  happy;                 /* ticks left of the "ooh!" expression   */
    uint8_t  expr_i;
    uint16_t expr_t;
    DoomoLook shown;                /* look on screen (throttled)            */
    uint8_t  shown_locked;          /* ... is a locked option: silhouette     */
    uint32_t shown_t, changed_t;
    CrSpark  spark[CR_SPARKS];
    uint8_t  pix[PORTRAIT_W * PORTRAIT_H];   /* portrait / fighter render    */
    DoomoImage img;
    uint8_t  pix_kind;              /* CRB_*                                 */
    uint32_t pix_key;

    /* voice test */
    const char *talk;
    uint8_t  talk_pos, talk_len;
    uint16_t talk_t;

    /* session counters (secret unlocks) */
    uint8_t  n_undo, n_voice, n_random;

    /* name entry, finishing, results */
    char     namebuf[DOOMO_NAME_LEN];
    char     nickbuf[DOOMO_NAME_LEN];
    char     nick[DOOMO_NAME_LEN];  /* draft nickname                        */
    char     nick_orig[DOOMO_NAME_LEN];
    uint8_t  finishing;             /* naming before moving in               */
    int8_t   preset_i, outfit_i, voice_i;
    int16_t  home_id;               /* slot a new citizen would move into    */
    int16_t  result;                /* id written, -1 cancelled              */
    char     msg[64];               /* move-in caption                       */
} CrState;

extern CrState cr;

/* ---- creator.c helpers used by the drawing code ---------------------------- */
const CrRow *cr_cur_row(void);
const CrRow *cr_row_at(int tab, int i);
int  cr_row_value(const DoomoResident *r, const CrRow *row);  /* index form */
void cr_row_set(DoomoResident *r, const CrRow *row, int index);
int  cr_row_count(const CrRow *row);                           /* choices    */
int  cr_value_locked(const CrRow *row, int index);
/* Text of choice `index` of a row (buf >= 48 bytes). */
const char *cr_choice_name(const CrRow *row, int index, char *buf);
/* Label of a row (the skin row says "Fur", "Goo", "Plating" ...). */
const char *cr_row_label(const CrRow *row);
/* Unlock hint for a locked choice ("Win 3 exorcisms"), "" if open. */
const char *cr_lock_hint(const CrRow *row, int index);
/* 1 if the preset uses a locked option; *hint gets its unlock hint. */
int  cr_preset_locked(const CrPreset *p, const char **hint);
int  cr_undo_count(void);
int  cr_redo_count(void);
int  cr_is_hellish(void);              /* Hellvile flavour for the visuals */
int  cr_dirty(void);                   /* draft differs from the original  */
const CrPreset *cr_action_preset(int action, int index);
int  cr_action_choices(int action);
int  cr_action_index(int action);
/* Apartment floor (1..50) and room (1..10) of a resident slot. */
void cr_home_floor_room(int id, int *floor, int *room);
const char *cr_menu_label(int i, char *buf);
int  cr_menu_enabled(int i);
/* copy picker: k-th entry's resident id / record / name */
int  cr_copy_id_at(int k);
const DoomoResident *cr_copy_res(int id);
/* Number of selectable rows above / the index of the selected row in
 * the tab (headers count as rows for layout). */
int  cr_first_selectable(int tab);
/* The look the preview should show this tick + its status / expression. */
int  cr_preview_status(void);
int  cr_preview_expr(void);
/* Spawn sparkles around the preview (kind -1 = mixed). */
void cr_sparkle_burst(int n, int kind);
/* Columns of the swatch grid of a colour row (8, or 11 with "None"). */
int  cr_color_cols(const CrRow *row);
/* public API (also declared in scenes.h) */
int  creator_last_result(void);
const char *creator_nickname(int id);

/* ---- creator_draw.c ------------------------------------------------------- */
void cr_draw(void);
void cr_layout(void);                   /* recompute rects for the screen  */
/* screen rects (filled by cr_layout) */
typedef struct CrLayout {
    int sw, sh, big;                    /* big = 320x240-class screen      */
    int px, py, pw, ph;                 /* preview pane                    */
    int lx, ly, lw, lh;                 /* list window                     */
    int tx, ty, tcell;                  /* tab strip                       */
    int rows_y, row_h, rows;            /* visible list rows               */
    int foot_y;                         /* detail line under the rows      */
    int fig_scale, ftr_scale, por_scale;/* figure / turntable / bust scale */
} CrLayout;
extern CrLayout crl;
/* Rows of the copy picker / option picker that fit. */
int  cr_pick_rows(void);

#endif
