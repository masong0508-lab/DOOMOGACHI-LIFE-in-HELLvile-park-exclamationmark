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
#include "doomo/part_icons.h"
#include "doomo/part_meta.h"
#include "doomo/look_fields.h"

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
                       list or CR_NOLIST; aux = 0 (the whole byte) or the
                       bit mask of a field that shares its byte (`romance`:
                       ROMANCE_PREF_MASK "Crushes on", LIFE_STAGE_MASK "Age") */
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
/* CR3: RK_PAINT rows' aux bits: a second row for a colour another row also edits (the outer
 * layer uses the accessory colour, legwear the print colour; the help line says so), and a row
 * whose preview is the legs close-up */
#define CR_AUX_SHARED 1
#define CR_AUX_LEGS   2

/* actions (RK_ACTION aux) */
enum { CA_SPECIES_PRESET, CA_OUTFIT_PRESET, CA_VOICE_PRESET, CA_TEST_VOICE,
       CA_MATCH_FUR, CA_COUNT };

typedef struct CrRow {
    uint8_t kind, field, count, base, clist, aux;
    const char *label;
    const char *const *names;
} CrRow;
/* The Age row (LS0): an RK_ENUM on the life-stage bits of `romance`. */
#define CR_IS_AGE_ROW(row) ((row)->kind == RK_ENUM && (row)->aux == LIFE_STAGE_MASK)

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
       CRP_MOVEIN, CRP_HERO, CRP_GRID };
/* START menu entries */
/* The menu only lists what applies right now (no greyed Undo (0), Reset on a clean citizen):
 * cr_menu_list() gives the visible entries in order. Randomise is one entry; LEFT / RIGHT on it
 * switches between everything and this tab. */
enum { CM_DONE, CM_RANDOM, CM_UNDO, CM_REDO, CM_COPY,
       CM_SPOOKY, CM_ROWS, CM_RESET, CM_QUIT, CM_COUNT };

/* CR9 (GBA speed step, docs/CREATOR.md "Speed"): the preview holds still between changes (no
 * blink / breathing frames, so the figure and the bust are not drawn again every few ticks; a new
 * expression is drawn in slices while the old one stays up), a new look or camera is drawn in slices
 * while the last one stays up (cr.disp / cr.pend), the stage's wallpaper, motes and backdrop stand
 * still and the preview pane is a retained panel. Elsewhere it animates as before. A variable so
 * the host tests can run the GBA's way too (test_panel.c); 1 on the GBA, 0 elsewhere (a constant 0
 * on the PS1, so none of it is built there: its resident RAM sits at the 72 KB plan gate). */
#if defined(DOOMO_PLATFORM_GBA)
#define CR_CALM 1
#elif defined(DOOMO_PLATFORM_PS1)
#define CR_CALM 0
#else
extern uint8_t cr_calm;
#define CR_CALM (cr_calm != 0)
#endif
/* CR9: the part of it the PS1 takes too: a new look or camera is drawn in slices while the last one
 * stays up, and lands with its hop (creator_draw.c pane_view); the PS1's preview still animates */
#if defined(DOOMO_PLATFORM_PS1)
#define CR_SLICE 1
#else
#define CR_SLICE CR_CALM
#endif

/* CR9: the creator's retained panels (platform.h plat_panel_begin; GBA). The ids are cache slots
 * of the overworld HUD's panels: the creator never draws under the overworld, so they are free
 * while it runs (a panel whose id, rect or key changed is simply drawn again). */
#define PANEL_CR_TABS PANEL_OW_OBJECTIVE
#define PANEL_CR_LIST PANEL_OW_CLOCK
#define PANEL_CR_PANE PANEL_OW_PROMPT
#define PANEL_CR_HINT PANEL_FP_HUD

/* CR6: how many rows the tabs show (START menu "Show"): the everyday rows, or every fine-tuning
 * slider and toggle too (cr_row_level). */
enum { CR_SHOW_SIMPLE, CR_SHOW_ALL, CR_SHOW_COUNT };

/* CR6: the preview's camera (cr_camera): the full figure, the bust (head slots), the legs (shoes,
 * bottoms) or the back (tails, wings) */
enum { CAM_FIGURE, CAM_BUST, CAM_LEGS, CAM_BACK };

/* CR6: the part grid (creator_grid.c): every choice of a part row as a picture */
#define CR_GRID_MAX   128       /* choices of one slot (gene widths <= 64, deep fields <= 127)    */
#define CR_GRID_FILT  24        /* filter chips of one slot                                      */
enum { GFK_ALL, GFK_OPEN, GFK_SHAPE, GFK_SET };           /* filter chip kinds (g_filt[] >> 5)     */

typedef struct CrSpark {
    int16_t x, y;           /* 1/16 px, relative to the preview's feet      */
    int8_t  vx, vy;
    uint8_t life, kind;     /* kind 0 sparkle, 1 heart, 2 star, 3 confetti  */
    uint8_t color;
} CrSpark;

/* CR9: a memo of cr_value_locked() (unlock_find walks the unlock tables; the list asks per row per
 * frame). Valid while the unlock bits are unchanged (cr.lmemo_sig). */
#define CR_LMEMO 32
typedef struct CrLockMemo { const struct CrRow *row; int16_t idx; uint8_t val, ok; } CrLockMemo;

/* CR9: what the preview pane shows: the look, the look without the browsed locked part (to show
 * the part alone in lavender) and the camera (CAM_*). memcmp-able (zeroed when unused). */
typedef struct CrView {
    DoomoLook look, base;
    DoomoDeep dl, db;               /* CR3: their gene-only fields (glasses, outer layer ...) */
    uint8_t locked, cam, ok, pad;
} CrView;

/* G2-CR: the creator's screen regions (each a retained panel on the GBA): on the held frame a region
 * is drawn only when its key changed, on two drawn ticks (the two pages), then left alone */
enum { CRR_TABS, CRR_LIST, CRR_PANE, CRR_HINT, CRR_COUNT };

/* what the shared pixel buffer currently holds */
enum { CRB_NONE, CRB_PORTRAIT, CRB_FIGHTER, CRB_LEGS };

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

    uint8_t  hero_sel;              /* PA2 first page: 0 Sunny 1 Marvin 2 own */
    uint8_t  tab, phase;
    uint8_t  row[CR_TAB_COUNT], top[CR_TAB_COUNT];
    int16_t  browse;                /* locked choice being looked at, or -1  */
    uint16_t tab_t, phase_t;        /* ticks since the tab / phase changed   */
    uint32_t t;                     /* ticks since the creator opened        */

    /* pickers */
    int16_t  pick_sel, pick_top, pick_orig;
    uint8_t  pick_err;              /* ticks of "locked!" shake              */
    uint8_t  menu_sel;              /* a CM_ id (not a row number)           */
    uint8_t  rand_tab;              /* the menu's Randomise scope: 0 all, 1 this tab */
    uint8_t  copy_src;              /* 0 town, 1 premade roster              */
    int16_t  copy_sel, copy_top, copy_count;
    int16_t  copy_memo_k, copy_memo_id;

    /* preview */
    uint8_t  spooky;                /* possessed preview                     */
    uint8_t  view8, walking, spin;  /* turntable                              */
    uint8_t  stick_peek, stick_idle; /* (ST1) turntable opened by the right stick */
    int16_t  stick_acc;             /* (ST1) right stick spin accumulator      */
    uint8_t  roll;                  /* ticks left of a turntable tumble      */
    uint8_t  hop;                   /* ticks left of the change hop          */
    uint8_t  happy;                 /* ticks left of the "ooh!" expression   */
    uint8_t  expr_i;
    uint16_t expr_t;
    DoomoLook shown;                /* look the preview wants (throttled but on the GBA) */
    DoomoLook shown_base;           /* ... without the browsed locked part (shown_locked) */
    DoomoDeep shown_d, shown_base_d;  /* CR3: their gene-only fields (look_gene_deep) */
    uint8_t  shown_locked;          /* ... is a locked option: silhouette     */
    uint8_t  pend_t;                /* CR9: ticks cr.pend has been drawn in slices */
    uint16_t row_at, row_t;         /* CR9: the row the cursor is on (tab << 8 | row), ticks there */
    uint8_t  sel_used;              /* CR6: SELECT was part of SELECT + UP / DOWN (no turntable) */
    CrView   disp, pend;            /* CR9: the view on screen / the one being drawn (CR_CALM) */
    uint32_t shown_t, changed_t;
    CrSpark  spark[CR_SPARKS];
    uint8_t  pix[PORTRAIT_W * PORTRAIT_H] __attribute__((aligned(4)));   /* portrait / fighter render */
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

    /* dimmed tab icons (creator_draw.c), made on first use each session */
    uint8_t  icon_pix[CR_TAB_COUNT][CR_ICON_W * CR_ICON_W];
    DoomoImage icon_img[CR_TAB_COUNT];

    /* CR6: rows shown (CR_SHOW_*) */
    uint8_t  show;
    /* CR9: the lock memo */
    CrLockMemo lmemo[CR_LMEMO];
    uint32_t lmemo_sig;

    /* CR6: the part grid (creator_grid.c) */
    uint8_t  g_slot;                /* doomo_part_slots index of the open row */
    uint8_t  g_n;                   /* choices listed (after the filter)      */
    uint8_t  g_filter, g_nfilt;     /* chip shown / chips of this slot        */
    uint8_t  g_filt[CR_GRID_FILT];  /* chip: kind << 5 | arg (GFK_*)          */
    int16_t  g_sel, g_top;          /* cursor (position in g_idx), first row  */
    uint8_t  g_idx[CR_GRID_MAX];    /* listed choices (row index form)        */
    uint16_t g_meta[CR_GRID_MAX];   /* their doomo_part_meta rows             */
    uint8_t  g_lock[CR_GRID_MAX / 8];  /* CR9: lock bit of every choice of the row, made on open */
    char     g_find[12];            /* search letters ("" none)               */
    char     g_findbuf[12];
    uint8_t  icon_buf[PART_ICON_PIX];  /* an unpacked icon (DOOMO_ICONS_RLE builds) */

    /* CR6: a locked part on the citizen, the part alone in lavender (creator_draw.c) */
    uint8_t  lockpix[FIGURE_W * FIGURE_H] __attribute__((aligned(4)));
    DoomoImage lockimg;
    uint32_t lockkey;
    /* CR9: lock_compose's draft picture (its uid, colours, opaque pixels) and the dark colours */
    uint32_t lc_uid;
    uint16_t lc_opaque;
    uint8_t  lc_has[32], lc_dark[32], lc_dark_ok;

    /* G2-CR: the held creator frame (creator_draw.c "the held frame"): what each region last drew */
    uint8_t  hd_in;                 /* this tick is drawn on the held frame      */
    uint8_t  hd_strip;              /* the button bar's rows have a clean copy   */
    uint8_t  hd_sig;                /* the backdrop the last full frame drew + 1 */
    uint8_t  hd_n[CRR_COUNT];       /* drawn ticks in a row with hd_key (2: both pages carry it) */
    uint32_t hd_key[CRR_COUNT];
    /* S3-3: what the platform may still keep: each region's key on the last drawn tick it was begun, and
     * that tick's number (pk_dn) in the count of drawn ticks (dn); a region is drawn before its panel
     * (cr_region_pre) when it is not what the last drawn tick showed */
    uint32_t pk_key[CRR_COUNT];
    uint16_t pk_dn[CRR_COUNT];
    uint16_t dn;
} CrState;

/* (integration, memory budget) The editor state (~6.4 KB with the undo
 * timeline and the preview buffer) lives in the shared scene scratch
 * (scene.h, tag CREA) while SCENE_CREATOR runs, not in .bss: it only means
 * something during a session, and the creator only opens modals on top of
 * itself. `cr` reads as before. */
#define CR_SCRATCH_TAG SAVE_EXT_TAG('C', 'R', 'E', 'A')
extern CrState *cr_sp;
#define cr (*cr_sp)

/* ---- creator.c helpers used by the drawing code ---------------------------- */
const CrRow *cr_cur_row(void);
const CrRow *cr_row_at(int tab, int i);
int  cr_row_value(const DoomoResident *r, const CrRow *row);  /* index form */
void cr_row_set(DoomoResident *r, const CrRow *row, int index);
int  cr_row_count(const CrRow *row);                           /* choices    */
int  cr_value_locked(const CrRow *row, int index);
/* CR9: cr_value_locked() keeps a memo; this clears it when the unlock bits changed (the creator
 * calls it once a tick, after the update and before the drawing) */
void cr_lock_memo_check(void);
/* CR12: 1 if choice `index` of a part row is above the draft's life stage (catalog age class:
 * TEEN+ for a baby / toddler / child, ADULT for any minor). Such a part is not offered at all:
 * LEFT / RIGHT and the picker skip it, the picker does not list it. A slot's default (0) never is. */
int  cr_value_hidden(const CrRow *row, int index);
/* visible choices of a row (cr_row_count minus the hidden ones), and the visible position of index */
int  cr_visible_count(const CrRow *row);
int  cr_visible_pos(const CrRow *row, int index);
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
int  cr_menu_enabled(int i);          /* is the entry listed right now?        */
int  cr_menu_list(uint8_t *out);       /* the listed CM_ ids in order, returns how many */
int  cr_menu_index(int cm);            /* row of a CM_ id in that list, -1 = not listed */
int  cr_menu_group(int cm);            /* entries of one group sit together     */
/* copy picker: k-th entry's resident id / record / name */
int  cr_copy_id_at(int k);
const DoomoResident *cr_copy_res(int id);
/* Number of selectable rows above / the index of the selected row in
 * the tab (headers count as rows for layout). */
int  cr_first_selectable(int tab);
/* The look the preview should show this tick + its status / expression. */
int  cr_preview_status(void);
void cr_lock_base(DoomoLook *out, DoomoDeep *dout);   /* CR6: the draft without the browsed part */
int  cr_preview_expr(void);
/* Spawn sparkles around the preview (kind -1 = mixed). */
void cr_sparkle_burst(int n, int kind);
/* Columns of the swatch grid of a colour row (8, or 11 with "None"). */
int  cr_color_cols(const CrRow *row);
/* ---- CR6 -------------------------------------------------------------------------------------- */
/* The catalog list of a part row (part_meta.h), -1 for a row that is not a part. */
int  cr_row_part_list(const CrRow *row);
/* doomo_part_slots index of a part row, -1 for other rows. */
int  cr_row_slot(const CrRow *row);
/* The part meta of choice `index` of a part row, NULL for other rows. */
const PartMeta *cr_choice_meta(const CrRow *row, int index);
/* CR6 modes: a row's level (0 everyday, 1 fine-tuning) and whether the tab shows row i now. */
int  cr_row_level(const CrRow *row);
int  cr_row_shown(int tab, int i);
/* One help line for a row (what it does, how to change it). */
const char *cr_row_help(const CrRow *row);
/* The section caption a row sits under ("Eyes"), "" if none. */
const char *cr_row_section(int tab, int i);
/* The preview camera for this tick (CAM_*). */
int  cr_camera(void);
/* picker plumbing shared by the list picker and the grid (creator.c) */
void cr_pick_preview(const CrRow *row, int index);
void cr_pick_close(const CrRow *row, int ok);
/* the grid (creator_grid.c) */
void cr_grid_open(const CrRow *row);
void cr_grid_update(void);
void cr_grid_draw(void);
void cr_grid_rebuild(void);
int  cr_grid_choice(int pos);           /* choice (row index form) at grid position pos, -1 */
const char *cr_grid_chip(int f, char *buf);   /* label of filter chip f */
void cr_grid_layout(int *cols, int *rows, int *cell, int *gx, int *gy);
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
/* The START menu window for the entries listed right now (the title pill sits 6 px above y). */
typedef struct CrMenuRect { int x, y, w, h, ih, gap; } CrMenuRect;
void cr_menu_rect(CrMenuRect *m);
/* drawing helpers shared with creator_grid.c (CR6) */
void cr_pill(int x, int y, int w, int h, uint8_t fill, uint8_t outline, uint8_t hi);
void cr_padlock(int x, int y, uint8_t c, uint8_t hole);
const char *cr_fit(const char *s, int maxw, char *buf);       /* buf >= 48 */
const char *cr_marquee(const char *s, int maxw, char *buf);   /* buf >= 48 */
void cr_draw_tab_icon(int x, int y, int tab);
/* G2-CR: plat_panel_begin for a creator region (CRR_*): 1 draw, 0 kept, 2 nothing to do (the held
 * frame carries it on both pages: return without plat_panel_end) */
int  cr_region(int r, int id, int x, int y, int w, int h, uint32_t key);
/* S3-3: the recipe of S2-6: a region whose key is new is drawn before its panel on the fast path, then taken as a
 * solid panel (creator_draw.c) */
int  cr_region_pre(int r, uint32_t key);
void cr_region_pre_end(int r, int id, int x, int y, int w, int h, uint32_t key, int solid, int hole_x, int hole_w);
/* Rows of the copy picker / option picker that fit. */
int  cr_pick_rows(void);

/* PA2 hero-card texts (creator_draw.c) */
const char *cr_hero_name(int i);
const char *cr_hero_tag(int i);

#endif
