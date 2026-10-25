/* home_draw.c - SCENE_HOME, the dream house scene (owner: H4; HF sec.2.1, 3,
 * 5, 10; SPEC sec.7 H4, C9, C14).
 *
 * One scene, five modes over one working room (HomeEdit, the undo ring):
 *   Build   the cursor on the grid: pick up / place / rotate / recolour
 *           (hold A on a piece, L / R per paint slot), the view brake, the
 *           rubber band (hold SELECT and move) with group moves, turns,
 *           copy / paste and "to storage"; SELECT taps undo
 *   Live    walk, A on a piece uses it (sit, sleep = the heartfelt fade,
 *           lamps on / off, examine), A on a door goes to the next room
 *   Photo   no HUD, L / R turn, UP / DOWN the daylight, SELECT the filter,
 *           B a sticker, A takes the 8-byte recipe (home_photo_add)
 *   Defend  Hold the Door (home_defend.c): the lanes, the door HP, A on the
 *           door / turret / a guest / a window, dawn and the result
 *   View    a photo re-rendered from its recipe (the gallery's photo wing)
 * The START menu's first three rows stay Catalog, the mode toggle, Score
 * (gba/harness/scripts/home.txt walks them); then Walls, Themes,
 * Blueprints, Photo, Rooms, Leave. A visit's menu: Give a gift, Photo,
 * Leave.
 *
 * Args (home.h, SPEC C14): -(1 + HOME_*) own home; 2 + id a visit
 * (home_visit.c generates the room; gifts, wants and reactions); 0x4000 |
 * HSM_* | param << 8 the story modes: TUTORIAL (Ep 1.2's checklist),
 * GOAL (param = HG_*), HAUNT_NIGHT (the night's reveal, the lights-out
 * insert, the 3 AM move), DEFEND (param = HDT_*), DECORATE_FOR (param =
 * layout id), OPEN_HOUSE (three visitors, the rating), PHOTO (param = the
 * photo slot). 0 and 1 are refused.
 *
 * Every animation of HF sec.5 goes through tween.h / camfx.h tables and
 * the timing table below (SPEC risk 6: no literal frame counts outside
 * it): the heart iris, the 2-frame anticipation, the hover bob, DROP soft
 * with corner dust, the neighbour ripple (one TW_JELLY spring sampled with
 * delays), the nope, the squash-flip, the view brake, the paint-splat and
 * paint-roller covered cuts, the group DROP stagger, the reverse DROP of
 * an undo, the blueprint montage (gaps 10 9 8 8 6 then 4), the expansion
 * rumble, the counter + SLAM stamp, the shutter, the cursed-piece insert
 * with the tape-stop, the heartfelt sleep, the vamp ducking under bubbles.
 *
 * State: the scene scratch ('HOME', sizeof(HomeScn) ~2.9 KB on 32-bit, SPEC
 * C9: the house's <= 4 KB); its tail is the view's plat_cache. Pushing
 * another scene (HomeMart, the bench) takes the scratch: the house then
 * re-enters its room on resume and the undo history is gone (documented).
 * Nothing here is saved except through home.c (commit on leave, on room
 * switches and after story results).
 */
#include "home_int.h"
#include "doomo/tween.h"
#include "doomo/camfx.h"
#include "doomo/audio.h"
#include "doomo/portrait.h"
#include "doomo/story_vm.h"
#include "doomo/font.h"
#include "../gen/furn_ids.h"

#define HD_TAG SAVE_EXT_TAG('H', 'O', 'M', 'E')

/* ---- the timing table (frames at 30 Hz; HF sec.5) ------------------------------- */
enum { HT_IRIS, HT_LIFT, HT_DROP_FALL, HT_DROP_LAND, HT_NOPE, HT_FLIP, HT_FLIP_SWAP,
       HT_BRAKE, HT_BRAKE_SHAKE, HT_MSG, HT_GHOST_PERIOD, HT_WALK_FRAME, HT_UNDO_STRETCH,
       HT_UNDO_POP, HT_RIPPLE_NEAR, HT_RIPPLE_FAR, HT_DUST, HT_SPLAT, HT_RUNNER,
       HT_RUNNER_SQUEAK, HT_IRIS_OUT, HT_IRIS_HOLD, HT_GROUP_GAP, HT_MONT_LAST,
       HT_STAMP, HT_COUNTER, HT_COUNTER_TICK, HT_SHUTTER, HT_HUD_POP, HT_INSERT,
       HT_INSERT_GAP, HT_FINE_AT, HT_HOLD_A, HT_BAND_HOLD, HT_RUMBLE, HT_RUMBLE_QUIET,
       HT_TILE_GAP, HT_REACT_LOOK, HT_REACT_SHOCK, HT_REACT_STARE, HT_REACT_BUBBLE,
       HT_REACT_COOL, HT_HOP, HT_HOP_GAP, HT_SLEEP_FADE, HT_SLEEP_MOON, HT_SLEEP_BLACK,
       HT_FREEZE_SIL, HT_FREEZE_POST, HT_LIGHTS_HOLD, HT_LIGHTS_FADE, HT_DUCK, HT_TAPE,
       HT_ARRIVE_GAP, HT_RATE_GAP, HT_BREATH, HT_BLINK_EVERY, HT_BLINK_LEN, HT_TV,
       HT_FAN, HT_BUBBLES, HT_GOAL_HOLD, HT_DAWN_CARD, HT_FLEE, HT_SIT_PLOP, HT_COUNT };
static const uint8_t ht[HT_COUNT] = {
    6,      /* HT_IRIS          heart iris in, r = 6 + 250 smooth(a/6)^2           */
    3,      /* HT_LIFT          the lift after a 2-frame anticipation              */
    6,      /* HT_DROP_FALL     DROP soft: t^2 fall                                */
    6,      /* HT_DROP_LAND     the s14 impact table                               */
    4,      /* HT_NOPE          x +2 -2 +2 -2                                      */
    5,      /* HT_FLIP          the squash-flip                                    */
    3,      /* HT_FLIP_SWAP     the edge-on frame                                  */
    10,     /* HT_BRAKE         the view rotate's hold                             */
    2,      /* HT_BRAKE_SHAKE   the shear wobble                                   */
    60,     /* HT_MSG           a caption stays                                    */
    39,     /* HT_GHOST_PERIOD  the breathing period (hover, bobs)                 */
    6,      /* HT_WALK_FRAME    chibi walk frame                                   */
    2,      /* HT_UNDO_STRETCH  undo: 0.92 / 1.10 ...                               */
    4,      /* HT_UNDO_POP      ... then up 24 px, (1-t)^2                         */
    2,      /* HT_RIPPLE_NEAR   neighbours sharing an edge, 0.4                    */
    4,      /* HT_RIPPLE_FAR    two tiles away, 0.15                               */
    2,      /* HT_DUST          the corner puffs                                   */
    3,      /* HT_SPLAT         the paint-splat covered cut                        */
    16,     /* HT_RUNNER        the paint-roller run, edge to edge                 */
    4,      /* HT_RUNNER_SQUEAK squeak every 4                                     */
    5,      /* HT_IRIS_OUT      the covered cut with nobody home: 5 out ...        */
    2,      /* HT_IRIS_HOLD     ... 2 black, 5 in                                  */
    2,      /* HT_GROUP_GAP     group DROP stagger                                 */
    4,      /* HT_MONT_LAST     the montage's last gap (10 9 8 8 6, then 4)        */
    14,     /* HT_STAMP         TW_SLAM_STAMP + hold                               */
    22,     /* HT_COUNTER       the counter rolls (a / 22)                         */
    2,      /* HT_COUNTER_TICK  a tick every 2 frames                              */
    1,      /* HT_SHUTTER       1 frame of white at 7/16                           */
    8,      /* HT_HUD_POP       the HUD pops back (TW_POP_BUBBLE)                  */
    4,      /* HT_INSERT        the hell insert                                    */
    2,      /* HT_INSERT_GAP    2 frames of nothing                                */
    8,      /* HT_FINE_AT       "...it's fine." at R8                              */
    12,     /* HT_HOLD_A        hold A on a piece: recolour                        */
    8,      /* HT_BAND_HOLD     hold SELECT: the rubber band                       */
    12,     /* HT_RUMBLE        the wall crumbles                                  */
    8,      /* HT_RUMBLE_QUIET  8 frames of silence                                */
    3,      /* HT_TILE_GAP      new floor tiles, diagonal by diagonal              */
    8,      /* HT_REACT_LOOK    the double-take: look                              */
    16,     /* HT_REACT_SHOCK   SHOCK with jitter                                  */
    8,      /* HT_REACT_STARE   stare at the lens                                  */
    50,     /* HT_REACT_BUBBLE  the bubble stays                                   */
    90,     /* HT_REACT_COOL    the density rule: one reaction, then a cooldown    */
    7,      /* HT_HOP           the s07 hop                                        */
    8,      /* HT_HOP_GAP       guests 8 frames apart                              */
    45,     /* HT_SLEEP_FADE    the heartfelt fade (30-60)                          */
    21,     /* HT_SLEEP_MOON    the room by moonlight                              */
    30,     /* HT_SLEEP_BLACK   black                                              */
    14,     /* HT_FREEZE_SIL    3 AM: 14 frames of silence                         */
    13,     /* HT_FREEZE_POST   ... the tik, 13 frames, the thaw                   */
    18,     /* HT_LIGHTS_HOLD   lights out: dead still                             */
    4,      /* HT_LIGHTS_FADE   BLDY to 16                                         */
    6,      /* HT_DUCK          the vamp ducks under a bubble                      */
    5,      /* HT_TAPE          the tape-stop when a cursed piece lands            */
    8,      /* HT_ARRIVE_GAP    open-house visitors 8 frames apart                 */
    15,     /* HT_RATE_GAP      the three raters, 15 frames apart                  */
    39,     /* HT_BREATH        idle breathing                                     */
    83,     /* HT_BLINK_EVERY   Kevin's third eye                                  */
    3,      /* HT_BLINK_LEN                                                        */
    2,      /* HT_TV            TV flicker                                         */
    2,      /* HT_FAN           the fan, 2 each                                    */
    11,     /* HT_BUBBLES       fish tank bubbles                                  */
    60,     /* HT_GOAL_HOLD     a met goal's stamp before the scene leaves          */
    90,     /* HT_DAWN_CARD     the defence's result card                          */
    16,     /* HT_FLEE          a frightened guest's 4-hop flee                    */
    2,      /* HT_SIT_PLOP      sitting: the plop squash                           */
};
static const int8_t nope_dx[4] = { 2, -2, 2, -2 };
static const int16_t flip_sx[6] = { 256, 128, 0, -128, -266, -256 };   /* Q8 */
static const uint8_t mont_gaps[5] = { 10, 9, 8, 8, 6 };               /* then HT_MONT_LAST */
static const uint8_t candle_gaps[3] = { 4, 5, 6 };                    /* co-prime with the breath */
#define DROP_FROM_PX 24          /* GBA px the DROP falls (HF sec.5.1)        */
#define LIFT_PX      6
#define UNDO_RISE_PX 24
#define HOP_PX       12

/* ---- tiny sprites for what has no chibi (the imps, the brute, Kevin, the pet) -- */
#define SPR_K 0
static const uint8_t spr_imp_px[10 * 12] = {
#define R PAL_PAINT(PAINT_BLOOD, 1)
#define D PAL_PAINT(PAINT_BLOOD, 0)
#define Y PAL_PAINT(PAINT_LEMON, 2)
#define K PAL_BLACK
    0,D,0,0,0,0,0,0,D,0,
    0,D,R,0,0,0,0,R,D,0,
    0,0,R,R,R,R,R,R,0,0,
    0,R,R,R,R,R,R,R,R,0,
    0,R,Y,K,R,R,Y,K,R,0,
    0,R,R,R,R,R,R,R,R,0,
    0,0,R,K,K,K,K,R,0,0,
    0,0,0,R,R,R,R,0,0,0,
    0,0,R,R,R,R,R,R,0,0,
    0,R,R,0,R,R,0,R,R,0,
    0,0,0,0,R,R,0,0,0,D,
    0,0,0,D,0,0,D,0,D,0,
};
static const uint8_t spr_brute_px[14 * 16] = {
    0,0,D,0,0,0,0,0,0,0,0,D,0,0,
    0,0,D,D,0,0,0,0,0,0,D,D,0,0,
    0,0,0,R,R,R,R,R,R,R,R,0,0,0,
    0,0,R,R,R,R,R,R,R,R,R,R,0,0,
    0,R,R,Y,Y,R,R,R,R,Y,Y,R,R,0,
    0,R,R,K,Y,R,R,R,R,Y,K,R,R,0,
    0,R,R,R,R,R,R,R,R,R,R,R,R,0,
    0,R,R,K,K,K,K,K,K,K,K,R,R,0,
    R,R,R,R,R,R,R,R,R,R,R,R,R,R,
    R,R,D,R,R,R,R,R,R,R,R,D,R,R,
    R,R,D,R,R,R,R,R,R,R,R,D,R,R,
    0,R,R,R,R,R,R,R,R,R,R,R,R,0,
    0,0,R,R,R,R,0,0,R,R,R,R,0,0,
    0,0,R,R,R,0,0,0,0,R,R,R,0,0,
    0,0,D,D,D,0,0,0,0,D,D,D,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};
#undef R
#undef D
#undef Y
#define G PAL_RAMP(RAMP_MINT, 4)
#define L PAL_RAMP(RAMP_MINT, 6)
#define W PAL_WHITE
static const uint8_t spr_kevin_px[10 * 10] = {
    0,0,0,G,G,G,G,0,0,0,
    0,0,G,L,L,L,L,G,0,0,
    0,G,L,W,K,K,W,L,G,0,
    0,G,L,L,W,K,L,L,G,0,
    G,L,W,K,L,L,W,K,L,G,
    G,L,L,L,L,L,L,L,L,G,
    G,L,L,K,K,K,K,L,L,G,
    0,G,L,L,L,L,L,L,G,0,
    0,0,G,G,L,L,G,G,0,0,
    0,0,0,G,0,0,G,0,0,0,
};
#undef G
#undef L
#define P PAL_PAINT(PAINT_BUBBLEGUM, 1)
#define Q PAL_PAINT(PAINT_BUBBLEGUM, 2)
static const uint8_t spr_pet_px[10 * 9] = {
    0,0,0,P,P,P,P,0,0,0,
    0,0,P,Q,Q,Q,Q,P,0,0,
    0,P,Q,K,Q,Q,K,Q,P,0,
    0,P,Q,Q,Q,Q,Q,Q,P,0,
    P,Q,Q,Q,K,K,Q,Q,Q,P,
    P,Q,Q,Q,Q,Q,Q,Q,Q,P,
    0,P,Q,Q,Q,Q,Q,Q,P,0,
    0,0,P,P,P,P,P,P,0,0,
    0,0,P,0,0,0,0,P,0,0,
};
#undef P
#undef Q
#undef K
#undef W
static const DoomoImage spr_imp = { 10, 12, spr_imp_px, 0xF0B00001u };
static const DoomoImage spr_brute = { 14, 16, spr_brute_px, 0xF0B00002u };
static const DoomoImage spr_kevin = { 10, 10, spr_kevin_px, 0xF0B00003u };
static const DoomoImage spr_pet = { 10, 9, spr_pet_px, 0xF0B00004u };

/* ---- state ---------------------------------------------------------------------- */
enum { HK_OWN, HK_VISIT, HK_STORY };
enum { HM_LIVE, HM_BUILD, HM_PHOTO, HM_DEFEND, HM_VIEW };
enum { HMN_NONE, HMN_MAIN, HMN_CATALOG, HMN_SCORE, HMN_WALLS, HMN_THEMES, HMN_BLUEPRINTS,
       HMN_ROOMS, HMN_GROUP };
enum { MI_CATALOG, MI_MODE, MI_SCORE, MI_WALLS, MI_THEMES, MI_BLUEPRINTS, MI_PHOTO, MI_ROOMS,
       MI_GIFT, MI_LEAVE, MI_COUNT };
static const char *const mi_names[MI_COUNT] = {
    "Catalog", "Live mode", "Score", "Walls", "Themes", "Blueprints", "Photo", "Rooms",
    "Give a gift", "Leave" };
enum { GI_MOVE, GI_COPY, GI_PASTE, GI_STORE, GI_COUNT };
static const char *const gi_names[GI_COUNT] = { "Done", "Copy", "Paste", "To storage" };
/* per-object effects (HomeScn.oa_k) */
enum { OA_NONE, OA_DROP, OA_RIPPLE_NEAR, OA_RIPPLE_FAR, OA_SPLAT, OA_HIDDEN };
/* actor slots */
enum { AC_PLAYER, AC_HOST, AC_PET, AC_RUNNER, AC_VISITOR0, AC_GUEST0 = AC_VISITOR0 + 3,
       AC_DEMON0 = AC_GUEST0 + HDG_COUNT, AC_MAX = AC_DEMON0 + 8 };
/* covered cuts for the walls / themes */
enum { CUT_NONE, CUT_WALLS, CUT_THEME, CUT_ROOM, CUT_BLUEPRINT };
/* the haunting night's beat */
enum { HN_NONE, HN_REVEAL, HN_LIGHTS, HN_INSERT, HN_AFTER, HN_3AM };
#define HD_GHOSTS 12

typedef struct HomeScn {
    HomeEdit ed;
    uint8_t  kind, mode, menu, view_k, room, story, param, done;
    int8_t   cx, cy;                 /* cursor tile (room space)                   */
    int16_t  held;                   /* furn in hand, -1 none                      */
    uint8_t  held_rot, held_from_room, held_pa, held_pb, held_sub;
    uint8_t  nope_age, flip_age, brake_age, lift_age, undo_age, shot_age, ins_age, exp_age;
    int8_t   brake_dir;
    uint16_t t, msg_age, a_hold, sel_hold;
    const char *msg;                 /* ROM strings or H->line                     */
    TwSpring cur_x, cur_y;           /* the cursor glide (TW_FOLLOW)               */
    TwSpring rip;                    /* the neighbour ripple (TW_JELLY)            */
    int16_t  rip_hist[8];
    uint8_t  rip_t;
    int8_t   oa_t[HOME_OBJ_MAX];     /* per-object effect clock (negative: waiting) */
    uint8_t  oa_k[HOME_OBJ_MAX];
    DoomoRoomObj undo_obj;           /* the piece an undo took away                 */
    fix16    px, py;                 /* the player chibi                           */
    uint8_t  pdir, pwalk, sitting, paint_mode, paint_slot;
    /* menus */
    uint8_t  mi[MI_COUNT], nmi;
    int16_t  sel, top, nrows;
    uint8_t  cat_tab, cat_tabs[FCAT_COUNT], ncat_tabs;
    uint16_t rows[128];              /* catalog / blueprint / room rows             */
    uint8_t  wall_row, wall_v[4];    /* the Walls panel: pattern, paint x 2         */
    /* the score panel */
    uint16_t score_gen;
    HomeScore score;
    uint8_t  sc_age, sc_grade, sc_last;
    /* covered cuts and montages */
    uint8_t  cut, cut_age, cut_arg, cut_v[4];
    uint8_t  mont_n, mont_i, mont_wait, mont_first;
    HomePlan plan;
    /* multi-select */
    uint8_t  band, band_x, band_y, nsel, sel_idx[HOME_SEL_MAX];
    HomeClip clip;
    /* visits and reactions */
    HomeVisit visit;
    uint8_t  react, react_age, react_cool, react_actor, gift_mode;
    const char *react_line;
    fix16    hx, hy;                 /* the host                                     */
    uint8_t  hdir, host_out;
    /* the pet */
    uint8_t  pet_on, pet_from, pet_to, pet_age, pet_beeped;
    /* photo mode / the viewer */
    HomePhoto photo;
    uint8_t  sticker_sel, view_slot;
    /* the haunting / 3 AM / lights out / sleep */
    uint8_t  hn, hn_age, sleep_age;
    /* open house and the rating */
    uint8_t  oh_n, oh_age, oh_ids_n;
    uint16_t oh_ids[3];
    /* Hold the Door */
    HomeDefend def;
    uint8_t  def_card;
    uint8_t  part;                   /* PLAT_PART_*: the visit / defence code (scene.h) */
    /* the view */
    DoomoRoomObj objs[HOME_OBJ_MAX + 1 + HD_GHOSTS];
    DoomoRoomActor act[AC_MAX];
    uint8_t  nact;
    char     line[48];
} HomeScn;

static HomeScn *H;
static int8_t resume_arg_set;        /* .bss: re-enter after the scratch was taken */
static int    resume_arg;

static const char *const stat_names[HS_COUNT] = { "Comfort", "Spook", "Style", "Light", "Barricade" };
static const char *const grade_names[HG_COUNT] = { "-", "COSY", "LIT", "HELD", "HOME", "ETERNAL" };
static const uint8_t stat_cols[HS_COUNT] = { PAL_UI_PINK, PAL_UI_PURPLE, PAL_UI_MINT, PAL_UI_GOLD, PAL_UI_RED };
static const char *const cat_names[FCAT_COUNT] = { "Pastel", "Gothic", "Hellish", "Plush", "Kitchen",
    "Tech", "Garden", "Light", "Wall", "Rug", "Trophy", "Season", "Story" };
static const char *const rule_text[13] = {
    "The fireplace can breathe", "The bed is not by the door", "The bed faces the door",
    "A lamp by the bed", "A plushie on the bed", "A mirror catches the window",
    "A pentagram under the bed", "Tables side by side", "Hill and Hell, in pairs",
    "Kitchen things outside the kitchen", "It does nothing", "Nobody can reach the bed",
    "Too cluttered" };
static const char *const sticker_names[4] = { "", STR_ICON_HEART, "it's fine.", STR_ICON_SKULL };
static const char *const filter_names[5] = { "No filter", "Storybook", "VHS", "Game Pal", "Sepia" };
static const uint8_t filter_pal[5] = { PAL_LERP_NONE, PAL_CRAYON, PAL_LERP_NONE, PAL_LILAC, PAL_SEPIA };

/* ---- helpers ------------------------------------------------------------------ */
static int room_w(void) { return HR_W(&H->ed.room); }
static int room_d(void) { return HR_D(&H->ed.room); }

void home_obj_view(const HomeObj *o, DoomoRoomObj *out)
{
    const DoomoFurn *f = home_furn(HO_FURN(o));
    int pa = o->pa & 0x1F, pb = o->pb & 0x1F;
    memset(out, 0, sizeof(*out));
    out->furn = HO_FURN(o);
    out->x = (uint8_t)HO_X(o);
    out->y = (uint8_t)HO_Y(o);
    out->z = (uint8_t)(o->z * 4);
    out->rot_anim = RO_ROT_ANIM(HO_ROT(o), 0);
    out->paint_a = f ? (pa == FURN_PAINT_DEFAULT ? f->paint_a : (uint8_t)pa) : 0xFF;
    out->paint_b = f ? (pb == FURN_PAINT_DEFAULT ? f->paint_b : (uint8_t)pb) : 0xFF;
    if (f && (f->flags & FF_LIGHT) && !(o->pa & HOS_OFF)) out->flags |= RO_FULLBRIGHT;
    if (HO_ONSURF(o)) out->flags |= (uint8_t)(((HO_SUB(o) & 1) ? RO_HALF_X : 0) | ((HO_SUB(o) & 2) ? RO_HALF_Y : 0));
}

/* view-space d-pad delta -> room space (inverse of the view's k turns) */
static void view_to_room(int k, int *dx, int *dy)
{
    int i, t;
    for (i = 0; i < k; i++) { t = *dx; *dx = *dy; *dy = -t; }
}

static int dpad(int *dx, int *dy)
{
    *dx = *dy = 0;
    if (btn_repeat(BTN_UP)) *dy = -1;
    else if (btn_repeat(BTN_DOWN)) *dy = 1;
    else if (btn_repeat(BTN_LEFT)) *dx = -1;
    else if (btn_repeat(BTN_RIGHT)) *dx = 1;
    if (!*dx && !*dy) return 0;
    view_to_room(H->view_k, dx, dy);
    return 1;
}

static void say(const char *m)
{
    H->msg = m;
    H->msg_age = 0;
}

static void nope(int rc)
{
    H->nope_age = 0;
    audio_sfx(SFX_ERROR);
    if (rc > 0 && rc < HPR_COUNT) say(home_nope_text[rc]);
}

static int night(void) { return doomo_town.minute < 6 * 60 || doomo_town.minute >= 20 * 60; }

static int light_level(void)
{
    int m = doomo_town.minute, l;
    const HomeRoomDef *d = &home_room_defs[H->room];
    if (H->kind == HK_VISIT && H->visit.daylight) return H->visit.daylight;
    if (H->mode == HM_PHOTO || H->mode == HM_VIEW) return 5 + H->photo.daylight * 10 / 7;
    if (m >= 7 * 60 && m < 18 * 60) l = 15;
    else if (m >= 5 * 60 && m < 7 * 60) l = 10;
    else if (m >= 18 * 60 && m < 20 * 60) l = 11;
    else l = 5;
    if (d->dim && l > d->dim) l = d->dim;
    if (H->hn == HN_LIGHTS || (H->sleep_age != 0xFF && H->sleep_age >= ht[HT_SLEEP_FADE])) l = 4;
    return l;
}

static unsigned score_ctx(void)
{
    unsigned c = night() ? HSC_NIGHT : 0;
    if (doomo_town.mode == TOWN_STORY) {
        if (doomo_town.story.chapter >= STORY_CH3) c |= HSC_CH3;
        if (doomo_town.story.chapter >= STORY_CH4) c |= HSC_CH4;
    }
    return c;
}

static void rescore(void)
{
    if (H->score_gen == H->ed.gen) return;
    H->score_gen = H->ed.gen;
    home_score_room(&H->ed.room, H->room, score_ctx(), &H->score);
}

static void commit(void)
{
    if (!H || H->kind == HK_VISIT || H->mode == HM_VIEW) return;
    home_room_store(H->room, &H->ed.room);
    home_refresh_cache(&H->ed.room, H->room);
}

static void clear_fx(void)
{
    memset(H->oa_t, 0, sizeof(H->oa_t));
    memset(H->oa_k, 0, sizeof(H->oa_k));
    H->nope_age = H->flip_age = H->brake_age = H->lift_age = H->undo_age = 0xFF;
    H->shot_age = H->ins_age = H->exp_age = H->sc_age = H->sleep_age = 0xFF;
    H->react_age = H->pet_age = 0xFF;
    H->msg_age = 0xFFFF;
    H->a_hold = 0xFFFF;
    H->score_gen = 0xFFFF;
    H->nsel = 0;
    H->band = 0;
    H->plan.nghost = 0;
}

/* ---- the projection (room_ref's rules; the GBA matches it exactly) ------------- */
typedef struct Proj { int ox, oy, k, W, D, w, d; } Proj;

static void proj_setup(Proj *p)
{
    int vw = plat_screen_w(), vh = plat_screen_h() - 24, wall_h = vh >= 200 ? 64 : 48, total;
    const HomeRoomDef *def = &home_room_defs[H->room];
    p->k = H->view_k & 3;
    p->w = room_w(); p->d = room_d();
    p->W = (p->k & 1) ? p->d : p->w;
    p->D = (p->k & 1) ? p->w : p->d;
    total = (def->kind == ROOMK_WALLS ? wall_h : 8) + (p->W + p->D) * 8;
    p->ox = vw / 2 - (p->W - p->D) * 8;
    if (total + 4 <= vh) p->oy = 12 + (vh - total) / 2 + (total - (p->W + p->D) * 8);
    else p->oy = 12 + vh - (p->W + p->D) * 8 - 4;
}

/* a point in room space (16.16 tiles, z px) to the screen */
static void proj(const Proj *p, fix16 x, fix16 y, int z, int *sx, int *sy)
{
    int k;
    for (k = 0; k < p->k; k++) { fix16 t = x; x = INT2FIX(p->w) - y; y = t; }
    *sx = p->ox + FIX2INT(x * 16) - FIX2INT(y * 16);
    *sy = p->oy + ((FIX2INT(x * 16) + FIX2INT(y * 16)) >> 1) - z;
}

static void tile_center(const Proj *p, int tx, int ty, int z, int *sx, int *sy)
{
    proj(p, INT2FIX(tx) + FIX_ONE / 2, INT2FIX(ty) + FIX_ONE / 2, z, sx, sy);
}

/* ---- per-object effects ------------------------------------------------------------ */
static void fx_drop(int idx, int delay)
{
    if (idx < 0 || idx >= HOME_OBJ_MAX) return;
    H->oa_k[idx] = OA_DROP;
    H->oa_t[idx] = (int8_t)-delay;
}

/* DROP lands: punch the ripple spring, neighbours ripple 2 / 4 frames later */
static void ripple_from(int idx)
{
    const HomeRoom *r = &H->ed.room;
    const DoomoFurn *f = home_furn(HO_FURN(&r->obj[idx]));
    int i, w, d, x0, y0;
    if (!f) return;
    home_footprint(f, HO_ROT(&r->obj[idx]), &w, &d);
    x0 = HO_X(&r->obj[idx]); y0 = HO_Y(&r->obj[idx]);
    tw_spring_punch(&H->rip, 16 * 16);
    for (i = 0; i < r->nobj && i < HOME_OBJ_MAX; i++) {
        const DoomoFurn *g = home_furn(HO_FURN(&r->obj[i]));
        int gw, gd, gx, gy, dx, dy, dist;
        if (i == idx || !g || (g->flags2 & (FF_NO_SQUASH | FF_PORTAL)) || (g->flags & FF_WALL) || H->oa_k[i]) continue;
        home_footprint(g, HO_ROT(&r->obj[i]), &gw, &gd);
        gx = HO_X(&r->obj[i]); gy = HO_Y(&r->obj[i]);
        dx = gx >= x0 + w ? gx - (x0 + w) : x0 >= gx + gw ? x0 - (gx + gw) : -1;
        dy = gy >= y0 + d ? gy - (y0 + d) : y0 >= gy + gd ? y0 - (gy + gd) : -1;
        dist = (dx < 0 ? 0 : dx + 1) + (dy < 0 ? 0 : dy + 1);
        if (dist == 1) { H->oa_k[i] = OA_RIPPLE_NEAR; H->oa_t[i] = 0; }
        else if (dist == 2 || dist == 3) { H->oa_k[i] = OA_RIPPLE_FAR; H->oa_t[i] = 0; }
    }
}

static void fx_tick(void)
{
    int i;
    int32_t v = tw_spring_step(&H->rip);
    H->rip_hist[H->rip_t & 7] = (int16_t)v;
    H->rip_t++;
    for (i = 0; i < HOME_OBJ_MAX; i++) {
        int k = H->oa_k[i], a;
        if (!k) continue;
        if (H->oa_t[i] < 120) H->oa_t[i]++;
        a = H->oa_t[i];
        switch (k) {
        case OA_DROP:
            if (a == ht[HT_DROP_FALL]) {
                const DoomoFurn *f = i < H->ed.room.nobj ? home_furn(HO_FURN(&H->ed.room.obj[i])) : NULL;
                audio_sfx(SFX_THUD);
                audio_sfx_synth(SFX_BOING);
                if (f && (f->flags2 & FF_CURSED) && H->kind != HK_VISIT) {
                    /* the cursed-piece insert: 4 frames of hell, the music
                     * muted for exactly those frames, the vamp tape-stops */
                    H->ins_age = 0;
                    camfx_shake(SHK_IMPACT);
                    audio_sfx_synth(SFX_GLITCH);
                    audio_tapestop(ht[HT_TAPE]);
                    audio_music_mute(ht[HT_INSERT]);
                }
                ripple_from(i);
            }
            if (a > ht[HT_DROP_FALL] + ht[HT_DROP_LAND] + ht[HT_DUST]) H->oa_k[i] = OA_NONE;
            break;
        case OA_RIPPLE_NEAR: case OA_RIPPLE_FAR:
            if (a > 8 + ht[HT_RIPPLE_FAR]) H->oa_k[i] = OA_NONE;
            break;
        case OA_SPLAT:
            if (a > ht[HT_SPLAT]) H->oa_k[i] = OA_NONE;
            break;
        default: break;
        }
    }
}

/* ---- catalog, menus --------------------------------------------------------------- */
static int seen_new(int furn)
{
    HomeState *s = home_state();
    return s && furn >= 0 && furn < FURN_MAX && ((s->seen_new[furn >> 3] >> (furn & 7)) & 1);
}

static void clear_new(int furn)
{
    HomeState *s = home_state();
    if (s && furn >= 0 && furn < FURN_MAX) s->seen_new[furn >> 3] &= (uint8_t)~(1 << (furn & 7));
}

static int avail(int furn)
{
    if (H->kind == HK_VISIT) return home_owned(furn) - home_placed(furn, NULL, -1);
    return home_edit_avail(&H->ed, furn);
}

static void build_catalog(void)
{
    int i, n = home_furn_count(), c;
    H->ncat_tabs = 0;
    for (c = 0; c < FCAT_COUNT; c++) {
        for (i = 0; i < n; i++) {
            const DoomoFurn *f = home_furn(i);
            if (f && f->cat == c && !(f->flags2 & (FF_PORTAL | FF_FIXED)) && avail(i) > 0) break;
        }
        if (i < n) H->cat_tabs[H->ncat_tabs++] = (uint8_t)c;
    }
    if (H->cat_tab >= H->ncat_tabs) H->cat_tab = 0;
    H->nrows = 0;
    if (!H->ncat_tabs) return;
    c = H->cat_tabs[H->cat_tab];
    for (i = 0; i < n && H->nrows < 128; i++) {
        const DoomoFurn *f = home_furn(i);
        if (f && f->cat == c && !(f->flags2 & (FF_PORTAL | FF_FIXED)) && avail(i) > 0) H->rows[H->nrows++] = (uint16_t)i;
    }
    if (H->sel >= H->nrows) H->sel = 0;
}

static void open_menu(int menu)
{
    H->menu = (uint8_t)menu;
    H->sel = H->top = 0;
    H->nrows = 0;
    if (menu == HMN_MAIN) {
        H->nmi = 0;
        if (H->kind == HK_VISIT) {
            if (H->visit.kind != HVK_SEALED && H->visit.kind != HVK_VACANT) H->mi[H->nmi++] = MI_GIFT;
            H->mi[H->nmi++] = MI_PHOTO;
            H->mi[H->nmi++] = MI_LEAVE;
        } else {
            H->mi[H->nmi++] = MI_CATALOG;
            H->mi[H->nmi++] = MI_MODE;
            H->mi[H->nmi++] = MI_SCORE;
            if (H->mode == HM_BUILD) {
                H->mi[H->nmi++] = MI_WALLS;
                H->mi[H->nmi++] = MI_THEMES;
                H->mi[H->nmi++] = MI_BLUEPRINTS;
            }
            H->mi[H->nmi++] = MI_PHOTO;
            if (H->kind == HK_OWN) H->mi[H->nmi++] = MI_ROOMS;
            H->mi[H->nmi++] = MI_LEAVE;
        }
        H->nrows = H->nmi;
    } else if (menu == HMN_CATALOG) {
        build_catalog();
    } else if (menu == HMN_THEMES) {
        H->nrows = THEME_COUNT;
    } else if (menu == HMN_BLUEPRINTS) {
        int i;
        for (i = 0; i < home_blueprint_count() && H->nrows < 126; i++)
            if (home_blueprint_name(i)) H->rows[H->nrows++] = (uint16_t)i;
        H->rows[H->nrows++] = 0x8000;                   /* save to slot 1 */
        H->rows[H->nrows++] = 0x8001;                   /* save to slot 2 */
    } else if (menu == HMN_ROOMS) {
        int r;
        for (r = 0; r < HOME_ROOM_COUNT; r++) if (home_room_open(r)) H->rows[H->nrows++] = (uint16_t)r;
    } else if (menu == HMN_WALLS) {
        const HomeRoomDef *d = &home_room_defs[H->room];
        const HomeRoom *r = &H->ed.room;
        H->wall_v[0] = (uint8_t)((r->wallpaper == 0xFF ? d->wallpaper : r->wallpaper) & 7);
        H->wall_v[1] = r->wall_paint == 0xFF ? d->wall_paint : r->wall_paint;
        H->wall_v[2] = (uint8_t)((r->floor == 0xFF ? d->floor : r->floor) & 7);
        H->wall_v[3] = r->floor_paint == 0xFF ? d->floor_paint : r->floor_paint;
        H->wall_row = 0;
        H->nrows = 5;
    } else if (menu == HMN_GROUP) {
        H->nrows = GI_COUNT;
    } else if (menu == HMN_SCORE) {
        rescore();
        H->sc_age = 0;
        H->sc_grade = (uint8_t)home_stamps_grade(H->score.stamps);
    }
    audio_sfx(SFX_SELECT);
}

static void hand_from_catalog(int furn)
{
    const DoomoFurn *f = home_furn(furn);
    if (!f) return;
    H->held = (int16_t)furn;
    H->held_rot = 0;
    H->held_from_room = 0;
    H->held_pa = H->held_pb = FURN_PAINT_DEFAULT;
    H->held_sub = 0;
    H->lift_age = 0;
    if (H->kind != HK_VISIT) H->mode = HM_BUILD;
    clear_new(furn);
    audio_sfx_synth(SFX_POP2);
}

/* ---- the covered cuts: the paint-roller runner or an iris ------------------------- */
static void start_cut(int cut, int arg)
{
    DoomoResident *me = town_res(DOOMO_PLAYER_ID);
    H->cut = (uint8_t)cut;
    H->cut_arg = (uint8_t)arg;
    H->cut_age = 0;
    H->cut_v[3] = 0;
    if (me && me->status != RES_EMPTY && cut != CUT_ROOM) {
        audio_sfx(SFX_WHOOSH);
    } else {
        H->cut_v[3] = 1;                                /* nobody home: the iris */
        camfx_iris_out(plat_screen_w() / 2, plat_screen_h() / 2, 0, 1, ht[HT_IRIS_HOLD]);
    }
}

static void enter_room(int room);

static void apply_cut(void)
{
    switch (H->cut) {
    case CUT_WALLS:
        home_edit_walls(&H->ed, H->cut_v[0], H->cut_v[2] & 7, H->cut_v[1], H->wall_v[3]);
        break;
    case CUT_THEME:
        home_edit_theme(&H->ed, H->cut_arg);
        break;
    case CUT_ROOM:
        commit();
        enter_room(H->cut_arg);
        break;
    default: break;
    }
}

static int cut_len(void)
{
    return H->cut_v[3] ? ht[HT_IRIS_OUT] * 2 + ht[HT_IRIS_HOLD] : ht[HT_RUNNER];
}

static void cut_tick(void)
{
    int mid;
    if (H->cut == CUT_NONE) return;
    mid = H->cut_v[3] ? ht[HT_IRIS_OUT] + ht[HT_IRIS_HOLD] / 2 : ht[HT_RUNNER] / 2;
    if (!H->cut_v[3] && H->cut_age % ht[HT_RUNNER_SQUEAK] == 0) audio_sfx(SFX_SQUEAK);
    if (H->cut_age == mid) apply_cut();
    if (H->cut_v[3] && H->cut_age == ht[HT_IRIS_OUT] + ht[HT_IRIS_HOLD])
        camfx_iris_in(plat_screen_w() / 2, plat_screen_h() / 2, 0, 1, 0);
    if (++H->cut_age > cut_len()) {
        if (!H->cut_v[3]) audio_sfx(SFX_THUD);
        H->cut = CUT_NONE;
        H->cut_v[3] = 0;
    }
}

/* ---- the blueprint montage --------------------------------------------------------- */
static void montage_start(void)
{
    int i;
    H->mont_first = H->plan.first;
    H->mont_n = H->plan.placed;
    H->mont_i = 0;
    H->mont_wait = 0;
    for (i = H->mont_first; i < H->mont_first + H->mont_n && i < HOME_OBJ_MAX; i++) {
        H->oa_k[i] = OA_HIDDEN;
        H->oa_t[i] = 0;
    }
}

static void montage_tick(void)
{
    if (H->mont_n == 0 || H->mont_i > H->mont_n) return;
    if (H->mont_wait) { H->mont_wait--; return; }
    if (H->mont_i < H->mont_n) {
        int idx = H->mont_first + H->mont_i;
        fx_drop(idx, 0);
        H->mont_wait = H->mont_i < 5 ? mont_gaps[H->mont_i] : ht[HT_MONT_LAST];
        H->mont_i++;
    } else {
        /* the last piece is down: the ghosts pop, the stamp */
        H->mont_i++;
        H->sc_age = 0;
        H->sc_grade = 0xFE;                              /* "BLUEPRINT" */
        audio_sfx_synth(SFX_FANFARE);
        if (H->plan.nghost) say("Grey pieces: buy them at HomeMart.");
        H->mont_n = 0;
    }
}

/* ---- rooms, gifts, the haunting ---------------------------------------------------- */
static void place_new_gifts(void)
{
    /* NEW gift pieces (price 0, not placed yet) land with a group DROP (house_api.c) */
    int i, n = home_furn_count(), k = 0;
    for (i = 0; i < n && k < 4; i++) {
        const DoomoFurn *f = home_furn(i);
        int x, y, idx = -1;
        if (!f || !(f->flags2 & FF_GIFT) || !seen_new(i) || (f->flags2 & (FF_PORTAL | FF_FIXED | FF_CURSED)) ||
            (f->flags & FF_WALL) || home_edit_avail(&H->ed, i) <= 0)
            continue;
        for (y = 1; y < room_d() && idx < 0; y++)
            for (x = 1; x < room_w() && idx < 0; x++)
                if (home_fit(&H->ed.room, H->room, i, x, y, 0, -1, NULL) == HPR_OK)
                    idx = home_edit_place(&H->ed, i, x, y, 0, -1, -1);
        if (idx >= 0) { fx_drop(idx, k * ht[HT_GROUP_GAP] + ht[HT_IRIS]); clear_new(i); k++; }
    }
    if (k) { H->ed.undo_n = 0; H->ed.undo_head = 0; say("Gifts arrived!"); }
}

static void enter_room(int room)
{
    H->room = (uint8_t)room;
    home_edit_begin(&H->ed, room);
    if (home_state()) home_state()->active_room = (uint8_t)room;
    clear_fx();
    H->cx = (int8_t)(room_w() / 2);
    H->cy = (int8_t)(room_d() / 2);
    tw_spring_init(&H->cur_x, TW_FOLLOW, H->cx * 16 * 16);
    tw_spring_init(&H->cur_y, TW_FOLLOW, H->cy * 16 * 16);
    H->px = INT2FIX(home_room_defs[room].door_at) + FIX_ONE / 2;
    H->py = FIX_ONE + FIX_ONE / 2;
    if (room == HOME_ROOM_CELLAR) audio_music(SONG_OMINOUS);
    else if (room == HOME_ROOM_GARDEN) audio_music(SONG_NONE);
    else audio_music(SONG_MINGLE);
}

/* the haunting's morning reveal and the eye (home.c applies the nights) */
static void haunt_open(void)
{
    int bits;
    if (H->room != HOME_ROOM_LIVING && H->room != HOME_ROOM_BEDROOM) return;
    bits = home_haunt_apply(&H->ed.room, H->room);
    if (!bits) return;
    H->ed.gen++;
    H->ed.undo_n = 0;
    H->ed.undo_head = 0;
    H->hn = HN_REVEAL;
    H->hn_age = 0;
    if (bits & HHA_PENTA) say("...that rug was not there yesterday.");
    else if (bits & HHA_EYE) say("The lamp is off. Was it?");
    else if (bits & HHA_CRATE) say("The crate moved. A little.");
}

/* ---- enter / leave ------------------------------------------------------------------ */
static void start_defend(int table)
{
    HomeRoom r = H->ed.room;
    H->part = PLAT_PART_HOME_DEFEND;            /* the PS1 loads home_defend.c's code */
    plat_code_part(H->part);
    home_defend_begin(&H->def, table, &r, H->room);
    H->mode = HM_DEFEND;
    H->def_card = 0;
    audio_music(SONG_INVASION);
}

/* scenes pushed straight from a runner script or a hub may find an empty
 * island: move the premade citizens in (as the library and the fair do) */
static void populate(void)
{
    if (town_count(-1) == 0) town_new(doomo_town.mode, doomo_town.seed ? doomo_town.seed : 1234u, TOWN_POP_PREMADE);
}

static int pick_visitors(void)
{
    /* three friends, highest friendship first (the open house, the raters) */
    int id, n = 0, j;
    for (id = 0; id < DOOMO_MAX_RESIDENTS; id++) {
        DoomoResident *r = town_res(id);
        if (!r || r->status != RES_ALIVE) continue;
        if (n < 3) { H->oh_ids[n++] = (uint16_t)id; continue; }
        for (j = 0; j < 3; j++)
            if (town_res(H->oh_ids[j])->level < r->level) { H->oh_ids[j] = (uint16_t)id; break; }
    }
    return n;
}

static void home_enter(int arg)
{
    int room = HOME_ROOM_LIVING;
    if (arg == 0 || arg == 1) {             /* reserved (SPEC C14): refused */
        plat_log("home: refused arg");
        H = NULL;
        return;
    }
    H = (HomeScn *)scene_scratch(HD_TAG, (uint32_t)sizeof(HomeScn));
    if (!H) return;
    memset(H, 0, sizeof(*H));
    clear_fx();                                 /* every effect clock idle (0xFF), whatever the path */
    resume_arg = arg;
    resume_arg_set = 1;
    camfx_reset();
    tw_spring_init(&H->rip, TW_JELLY, 0);
    H->held = -1;
    H->mode = HM_BUILD;
    if (HOME_ARG_IS_OWN(arg)) {
        HomeState *s = home_state();
        H->kind = HK_OWN;
        if (s && !home_moved_in()) home_move_in();
        home_story_rooms();
        if (s && arg == HOME_ARG_OWN(HOME_COTTAGE) && home_room_open(s->active_room)) room = s->active_room;
        else if (arg == HOME_ARG_OWN(HOME_TOWERS_7B)) room = HOME_ROOM_TOWERS_7B;
        else if (arg == HOME_ARG_OWN(HOME_PITS_FLAT)) room = HOME_ROOM_PITS_FLAT;
        else if (arg == HOME_ARG_OWN(HOME_PARK_PLOT)) room = HOME_ROOM_PARK_PLOT;
        enter_room(room);
        haunt_open();
        place_new_gifts();
        if (home_mart_expanded == room) {               /* Town Hall just knocked a wall through */
            home_mart_expanded = 0xFF;
            H->exp_age = 0;
            camfx_shake(SHK_RUMBLE);
            audio_music_mute(ht[HT_RUMBLE] + ht[HT_RUMBLE_QUIET]);
        }
    } else if (HOME_ARG_IS_VISIT(arg)) {
        int id = (arg - 2) % 1024, as = (arg - 2) / 1024;
        H->kind = HK_VISIT;
        H->mode = HM_LIVE;
        populate();
        H->part = PLAT_PART_HOME_VISIT;         /* the PS1 loads home_visit.c's code */
        plat_code_part(H->part);
        home_visit_gen_as(id, as, &H->ed.room, &H->visit);
        H->room = H->visit.room;
        H->ed.room_id = H->room;
        clear_fx();
        H->cx = (int8_t)(room_w() / 2);
        H->cy = (int8_t)(room_d() / 2);
        tw_spring_init(&H->cur_x, TW_FOLLOW, H->cx * 16 * 16);
        tw_spring_init(&H->cur_y, TW_FOLLOW, H->cy * 16 * 16);
        H->px = INT2FIX(home_room_defs[H->room].door_at) + FIX_ONE / 2;
        H->py = FIX_ONE / 2;
        H->hx = INT2FIX(2) + FIX_ONE / 2;
        H->hy = INT2FIX(3) + FIX_ONE / 2;
        H->host_out = (uint8_t)(H->visit.kind == HVK_SEALED || H->visit.kind == HVK_AWAY || H->visit.kind == HVK_VACANT);
        home_visit_log(id);
        audio_music(H->visit.music);
        if (!H->host_out) {
            DoomoResident *r = town_res(id);
            H->react = (uint8_t)(r && r->persona[PERS_EXPRESSION] >= 10 ? HVR_DOUBLE_TAKE : HVR_DAWNING);
            H->react_age = 0;
            H->react_actor = AC_HOST;
            H->react_line = H->visit.kind == HVK_POSSESSED ? "...come in. Stay." :
                            H->visit.kind == HVK_GHOST ? "Oh. You can see me?" :
                            r && r->persona[PERS_SPEECH] >= 8 ? "Oh. It's you. Come in." : "Welcome! Come in!";
        } else if (H->visit.kind == HVK_AWAY) {
            DoomoResident *r = town_res(id);
            doomo_snprintf(H->line, sizeof(H->line), "A note: \"Back soon! %s\"",
                           r ? doomo_catchphrases[r->catchphrase % CATCHPHRASE_COUNT] : "");
            say(H->line);
        }
    } else {
        int mode = HOME_ARG_STORY_MODE(arg), param = HOME_ARG_STORY_PARAM(arg);
        H->kind = HK_STORY;
        H->story = (uint8_t)mode;
        H->param = (uint8_t)param;
        if (home_state() && !home_moved_in()) home_move_in();
        home_story_rooms();
        if (mode == HSM_HAUNT_NIGHT && param >= 1 && param <= 3) home_haunt_step(param);
        if (mode == HSM_TUTORIAL && !home_owned(FURN_HONEY_LAMP) && !home_owned(FURN_CUPCAKE_LAMP))
            home_grant(FURN_HONEY_LAMP);                 /* Nana's lamp, part of Ep 1.2 */
        if (mode == HSM_OPEN_HOUSE) populate();
        enter_room(mode == HSM_TUTORIAL || mode == HSM_HAUNT_NIGHT || mode == HSM_DEFEND ? HOME_ROOM_LIVING :
                   (home_state() && home_room_open(home_state()->active_room) ? home_state()->active_room : HOME_ROOM_LIVING));
        if (mode == HSM_DEFEND) {
            start_defend(param & 15);
            if (((param >> 4) & 3) < H->def.nwaves) H->def.wave = (uint8_t)((param >> 4) & 3);
        } else if (mode == HSM_HAUNT_NIGHT) {
            H->mode = HM_LIVE;
            haunt_open();
            if (H->hn == HN_NONE) { H->hn = HN_LIGHTS; H->hn_age = 0; }
            audio_music(SONG_OMINOUS);
        } else if (mode == HSM_PHOTO) {
            HomePhoto p;
            H->mode = HM_VIEW;
            H->view_slot = (uint8_t)param;
            if (home_photo_get(param, &p)) {
                H->photo = p;
                H->view_k = (uint8_t)(p.view & 3);
                if (p.place >= HOME_PHOTO_OWN && p.place - HOME_PHOTO_OWN < HOME_ROOM_COUNT) {
                    H->room = (uint8_t)(p.place - HOME_PHOTO_OWN);
                    home_room_load(H->room, &H->ed.room);
                } else if (p.place < DOOMO_MAX_RESIDENTS) {
                    H->part = PLAT_PART_HOME_VISIT;
                    plat_code_part(H->part);
                    home_visit_gen(p.place, &H->ed.room, &H->visit);
                    H->room = H->visit.room;
                }
            }
        } else if (mode == HSM_OPEN_HOUSE) {
            H->mode = HM_LIVE;
            H->oh_ids_n = (uint8_t)pick_visitors();
            H->oh_age = 0;
        } else if (mode == HSM_TUTORIAL) {
            place_new_gifts();
        }
    }
    H->view_k = H->mode == HM_VIEW ? H->view_k : 0;
    if (H->mode == HM_VIEW && H->photo.filter < 5 && filter_pal[H->photo.filter])
        camfx_pal_lerp(filter_pal[H->photo.filter], 8);
    if (H->kind == HK_OWN || H->kind == HK_STORY) {
        /* the pet lives in the house (HF sec.5.2) */
        H->pet_on = (uint8_t)(doomo_town.pet.stage > PET_STAGE_EGG && doomo_town.pet.stage < PET_STAGE_DEAD);
    }
    audio_sfx_synth(SFX_SHWIP);
}

static void home_leave(void)
{
    if (H && scene_scratch_mine(HD_TAG)) {
        plat_code_part(H->part);
        commit();
    }
    camfx_reset();
    H = NULL;
    resume_arg_set = 0;
}

static void leave_now(void)
{
    commit();
    if (H->kind == HK_VISIT && H->visit.kind == HVK_SEALED) audio_music(SONG_NONE);
    camfx_reset();
    resume_arg_set = 0;
    scene_pop();
}

/* ---- Build mode -------------------------------------------------------------------- */
static void place_held(void)
{
    int rc;
    if (H->kind == HK_VISIT) {
        HomeFit fit;
        rc = home_fit(&H->ed.room, H->room, H->held, H->cx, H->cy, H->held_rot, -1, &fit);
        if (rc != HPR_OK) { nope(rc); return; }
        if (home_visit_gift(H->visit.id, H->held, H->cx, H->cy, H->held_rot,
                            H->held_pa == FURN_PAINT_DEFAULT ? -1 : H->held_pa) < 0) {
            nope(HPR_FULL);
            return;
        }
        {
            HomeObj *o = &H->ed.room.obj[H->ed.room.nobj];
            const DoomoResident *r = town_res(H->visit.id);
            int thanks = -1, met;
            o->w0 = HO_W0(H->held, H->held_rot, fit.on_surface);
            o->pos = HO_POS(H->cx, H->cy);
            o->z = fit.z;
            o->pa = H->held_pa;
            o->pb = FURN_PAINT_DEFAULT;
            fx_drop(H->ed.room.nobj, 0);
            H->ed.room.nobj++;
            H->ed.gen++;
            met = home_visit_thank(H->visit.id, H->held, H->held_pa == FURN_PAINT_DEFAULT ? -1 : H->held_pa, &thanks);
            if (!H->react_cool && r && !H->host_out) {
                H->react = (uint8_t)home_visit_react(r, H->held, H->held_pa == FURN_PAINT_DEFAULT ? -1 : H->held_pa,
                                                     &H->react_line);
                H->react_age = 0;
                H->react_actor = AC_HOST;
                H->react_cool = ht[HT_REACT_COOL];
            }
            if (thanks >= 0) say("All three wishes! They gave you a present.");
            else if (met) say("That's what they wanted!");
        }
        H->held = -1;
        H->mode = HM_LIVE;
        audio_sfx(SFX_WHOOSH);
        return;
    }
    rc = home_edit_place_sub(&H->ed, H->held, H->cx, H->cy, H->held_rot, H->held_sub,
                             H->held_pa == FURN_PAINT_DEFAULT ? -1 : H->held_pa,
                             H->held_pb == FURN_PAINT_DEFAULT ? -1 : H->held_pb);
    if (rc >= 0) {
        fx_drop(rc, 0);
        H->held = -1;
        audio_sfx(SFX_WHOOSH);
        if (H->story == HSM_DECORATE_FOR && H->kind == HK_STORY) home_layout_progress(&H->ed.room, H->param, -1);
    } else {
        nope(-rc);
    }
}

static void pick_up_at(void)
{
    int idx = home_obj_at(&H->ed.room, H->cx, H->cy);
    if (idx < 0) return;
    {
        HomeObj o = H->ed.room.obj[idx];
        int furn = home_edit_pickup(&H->ed, idx);
        if (furn >= 0) {
            H->held = (int16_t)furn;
            H->held_rot = (uint8_t)HO_ROT(&o);
            H->held_from_room = 1;
            H->held_pa = o.pa & 0x1F;
            H->held_pb = o.pb & 0x1F;
            H->held_sub = (uint8_t)HO_SUB(&o);
            H->lift_age = 0;
            memset(H->oa_k, 0, sizeof(H->oa_k));         /* indices moved */
            audio_sfx_synth(SFX_POP2);
        } else {
            nope(-furn);
        }
    }
}

static void do_undo(void)
{
    HomeRoom before = H->ed.room;
    int i;
    if (!home_edit_undo(&H->ed)) { nope(0); return; }
    memset(H->oa_k, 0, sizeof(H->oa_k));
    H->undo_age = 0;
    audio_sfx_synth(SFX_SHWIP);
    if (H->ed.room.nobj < before.nobj) {
        /* the reverse DROP: the piece that went away pops up into nothing */
        for (i = 0; i < before.nobj; i++)
            if (i >= H->ed.room.nobj || memcmp(&before.obj[i], &H->ed.room.obj[i], sizeof(HomeObj))) {
                home_obj_view(&before.obj[i], &H->undo_obj);
                break;
            }
    } else {
        H->undo_age = 0xFF;
        for (i = 0; i < H->ed.room.nobj; i++)
            if (i >= before.nobj || memcmp(&before.obj[i], &H->ed.room.obj[i], sizeof(HomeObj))) fx_drop(i, 0);
    }
}

static void band_mask(DoomoRoomView *v)
{
    int x0 = H->band_x, y0 = H->band_y, x1 = H->cx, y1 = H->cy, x, y, t;
    if (x0 > x1) { t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; }
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            int bit = y * room_w() + x;
            if (bit < 128) v->sel_mask[bit >> 3] |= (uint8_t)(1 << (bit & 7));
        }
}

static void group_landed(void)
{
    int k;
    memset(H->oa_k, 0, sizeof(H->oa_k));
    for (k = 0; k < H->nsel; k++) fx_drop(H->sel_idx[k], k * ht[HT_GROUP_GAP]);
}

static void update_group(void)
{
    int dx, dy, rc;
    if (dpad(&dx, &dy)) {
        rc = home_edit_group_move(&H->ed, H->sel_idx, H->nsel, dx, dy);
        if (rc > 0) group_landed(); else nope(-rc);
    } else if (btn_pressed(BTN_L) || btn_pressed(BTN_R)) {
        rc = home_edit_group_rotate(&H->ed, H->sel_idx, H->nsel, btn_pressed(BTN_R) ? 1 : -1);
        if (rc > 0) { group_landed(); audio_sfx(SFX_WHOOSH); } else nope(-rc);
    } else if (btn_pressed(BTN_A) || btn_pressed(BTN_B)) {
        H->nsel = 0;
        audio_sfx(SFX_SELECT);
    } else if (btn_pressed(BTN_START)) {
        open_menu(HMN_GROUP);
    }
}

static void update_build(void)
{
    int dx, dy;
    const HomeObj *under;
    if (H->nsel) { update_group(); return; }
    /* hold SELECT: the rubber band */
    if (btn_down(BTN_SELECT) && H->held < 0) {
        if (++H->sel_hold == ht[HT_BAND_HOLD]) { H->band = 1; H->band_x = (uint8_t)H->cx; H->band_y = (uint8_t)H->cy; audio_sfx(SFX_BLIP); }
    }
    if (btn_released(BTN_SELECT)) {
        if (H->band) {
            H->nsel = (uint8_t)home_edit_select(&H->ed, H->band_x, H->band_y, H->cx, H->cy, H->sel_idx, HOME_SEL_MAX);
            H->band = 0;
            if (H->nsel) say("D-pad move  L/R turn  START more"); else audio_sfx(SFX_CANCEL);
        } else if (H->held < 0) {
            do_undo();
        }
        H->sel_hold = 0;
    }
    if (dpad(&dx, &dy)) {
        int nx = H->cx + dx, ny = H->cy + dy;
        if (nx >= 0 && ny >= 0 && nx < room_w() && ny < room_d()) {
            H->cx = (int8_t)nx;
            H->cy = (int8_t)ny;
            audio_sfx(SFX_CURSOR);
        }
    }
    if (H->band) return;
    under = NULL;
    {
        int idx = home_obj_at(&H->ed.room, H->cx, H->cy);
        if (idx >= 0) under = &H->ed.room.obj[idx];
    }
    /* recolour: hold A on a placed piece, L / R per paint slot */
    if (H->paint_mode) {
        int idx = home_obj_at(&H->ed.room, H->cx, H->cy);
        if (!btn_down(BTN_A) || idx < 0) { H->paint_mode = 0; H->a_hold = 0xFFFF; return; }
        if (btn_pressed(BTN_L) || btn_pressed(BTN_R)) {
            HomeObj *o = &H->ed.room.obj[idx];
            const DoomoFurn *f = home_furn(HO_FURN(o));
            int slot = btn_pressed(BTN_R), ra = o->pa & 0x1F, rb = o->pb & 0x1F, cur;
            if (!f || (slot ? f->paint_b : f->paint_a) == 0xFF) { nope(0); return; }
            cur = slot ? (rb == FURN_PAINT_DEFAULT ? f->paint_b : rb) : (ra == FURN_PAINT_DEFAULT ? f->paint_a : ra);
            cur = (cur + 1) % 32;
            if (slot) rb = cur; else ra = cur;
            if (home_edit_paint(&H->ed, idx, ra == FURN_PAINT_DEFAULT ? -1 : ra, rb == FURN_PAINT_DEFAULT ? -1 : rb)) {
                H->oa_k[idx] = OA_SPLAT;
                H->oa_t[idx] = 0;
                audio_sfx_pitch(SFX_BUBBLE, -5);
            }
        }
        return;
    }
    /* A: place what is held; empty-handed, a tap picks up (on release) and
     * a hold on a piece starts the recolour */
    if (btn_pressed(BTN_A) && H->held >= 0) {
        place_held();
        H->a_hold = 0xFFFF;                             /* this press is spent */
        return;
    }
    if (btn_pressed(BTN_A)) H->a_hold = 0;
    if (btn_down(BTN_A) && H->a_hold != 0xFFFF && H->held < 0 && under) {
        if (++H->a_hold >= ht[HT_HOLD_A]) {
            H->paint_mode = 1;
            H->a_hold = 0xFFFF;
            say("L: paint A   R: paint B");
            audio_sfx(SFX_SELECT);
            return;
        }
    }
    if (btn_released(BTN_A)) {
        if (H->a_hold != 0xFFFF && H->held < 0) pick_up_at();
        H->a_hold = 0xFFFF;
    } else if (btn_pressed(BTN_B)) {
        if (H->held >= 0) {
            if (H->held_from_room) do_undo();             /* put it back */
            H->held = -1;
            audio_sfx(SFX_CANCEL);
        } else if (H->kind == HK_VISIT) {
            H->mode = HM_LIVE;
        } else {
            leave_now();
        }
    } else if (btn_pressed(BTN_L) || btn_pressed(BTN_R)) {
        int dir = btn_pressed(BTN_R) ? 1 : -1;
        if (H->held >= 0) {
            H->held_rot = (uint8_t)((H->held_rot + dir) & 3);
            H->flip_age = 0;
            audio_sfx(SFX_WHOOSH);
        } else if (H->brake_age == 0xFF) {
            H->brake_age = 0;
            H->brake_dir = (int8_t)dir;
            audio_sfx(SFX_WHOOSH);
        }
    }
}

/* ---- Live mode ------------------------------------------------------------------------ */
static int walkable(int tx, int ty)
{
    int idx;
    const DoomoFurn *f;
    if (tx < 0 || ty < 0 || tx >= room_w() || ty >= room_d()) return 0;
    idx = home_obj_at(&H->ed.room, tx, ty);
    if (idx < 0) return 1;
    f = home_furn(HO_FURN(&H->ed.room.obj[idx]));
    return !f || (f->flags & (FF_RUG | FF_WALL | FF_SMALL)) != 0;
}

static void start_sleep(void)
{
    DoomoResident *me = town_res(DOOMO_PLAYER_ID);
    H->sleep_age = 0;
    audio_music_duck(0, ht[HT_SLEEP_FADE] + ht[HT_SLEEP_MOON] + ht[HT_SLEEP_BLACK]);
    camfx_fade_to(16, ht[HT_SLEEP_FADE]);
    if (me) me->stat[STAT_ENERGY] = 100;
}

static void live_use(void)
{
    /* A on what is in front of the player */
    int tx = FIX2INT(H->px), ty = FIX2INT(H->py), k, idx = -1;
    static const int8_t fdx[4] = { 0, -1, 1, 0 }, fdy[4] = { 1, 0, 0, -1 };
    if (H->kind == HK_VISIT && !H->host_out) {
        int hx = FIX2INT(H->hx), hy = FIX2INT(H->hy), ddx = hx - tx, ddy = hy - ty;
        if (ddx * ddx + ddy * ddy <= 2) {
            DoomoResident *r = town_res(H->visit.id);
            uint8_t w[3];
            home_visit_wants(H->visit.id, w);
            doomo_snprintf(H->line, sizeof(H->line), "I wish I had %s.", home_visit_want_text(w[(H->t >> 4) % 3]));
            H->react = HVR_DAWNING;
            H->react_age = 0;
            H->react_actor = AC_HOST;
            H->react_line = H->line;
            (void)r;
            return;
        }
    }
    for (k = 0; k < 2 && idx < 0; k++) {
        int x = tx + fdx[H->pdir & 3] * k, y = ty + fdy[H->pdir & 3] * k;
        idx = home_obj_at(&H->ed.room, x, y);
        if (idx < 0 && (x <= 0 || y <= 0)) {             /* the wall right there */
            if (y <= 0) idx = home_obj_at(&H->ed.room, x, 0);
            if (idx < 0 && x <= 0) idx = home_obj_at(&H->ed.room, 0, y);
        }
    }
    if (idx < 0) return;
    {
        const DoomoFurn *f = home_furn(HO_FURN(&H->ed.room.obj[idx]));
        if (!f) return;
        if (f->role == FR_DOOR) {
            if (H->kind == HK_VISIT) { leave_now(); return; }
            if (H->kind == HK_OWN) {
                int r = H->room, n;
                for (n = 0; n < HOME_COTTAGE_ROOMS; n++) {
                    r = (r + 1) % HOME_COTTAGE_ROOMS;
                    if (home_room_open(r)) break;
                }
                if (r != H->room) { start_cut(CUT_ROOM, r); return; }
            }
            leave_now();
            return;
        }
        if ((f->flags & FF_LIGHT) && H->kind != HK_VISIT) {
            home_edit_lights(&H->ed, idx);
            audio_sfx(SFX_BLIP);
            return;
        }
        if (f->interact == FI_SLEEP && H->kind != HK_VISIT) { start_sleep(); return; }
        if (f->interact == FI_SIT) {
            H->px = INT2FIX(HO_X(&H->ed.room.obj[idx])) + FIX_ONE / 2;
            H->py = INT2FIX(HO_Y(&H->ed.room.obj[idx])) + FIX_ONE / 2;
            H->sitting = 1;
            H->oa_k[idx] = OA_RIPPLE_NEAR;                /* the cushion gives */
            H->oa_t[idx] = 0;
            tw_spring_punch(&H->rip, 10 * 16);
            if (f->comfy >= 6) say(STR_ICON_HEART " So comfy.");
            if (HO_FURN(&H->ed.room.obj[idx]) == FURN_BIG_CHAIR) audio_sfx_synth(SFX_CREAK);
            return;
        }
        say(doomo_furn_text(f->desc, H->line, sizeof(H->line)));
    }
}

static void update_live(void)
{
    int dx = 0, dy = 0;
    if (btn_down(BTN_UP)) dy = -1;
    else if (btn_down(BTN_DOWN)) dy = 1;
    else if (btn_down(BTN_LEFT)) dx = -1;
    else if (btn_down(BTN_RIGHT)) dx = 1;
    if (dx || dy) {
        fix16 nx, ny;
        int vdx = dx, vdy = dy;
        H->sitting = 0;
        view_to_room(H->view_k, &dx, &dy);
        nx = H->px + dx * (FIX_ONE / 10);
        ny = H->py + dy * (FIX_ONE / 10);
        if (walkable(FIX2INT(nx + dx * (FIX_ONE / 4)), FIX2INT(ny + dy * (FIX_ONE / 4)))) { H->px = nx; H->py = ny; }
        H->pwalk++;
        H->pdir = (uint8_t)(vdy > 0 ? 0 : vdx < 0 ? 1 : vdx > 0 ? 2 : 3);
    } else {
        H->pwalk = 0;
    }
    if (btn_pressed(BTN_A)) live_use();
    else if (btn_pressed(BTN_B)) leave_now();
}

/* ---- Photo mode / the viewer ------------------------------------------------------------ */
static void photo_begin(void)
{
    memset(&H->photo, 0, sizeof(H->photo));
    H->photo.place = (uint16_t)(H->kind == HK_VISIT ? H->visit.id : HOME_PHOTO_OWN + H->room);
    H->photo.view = H->view_k;
    H->photo.daylight = (uint8_t)DOOMO_CLAMP((light_level() - 5) * 7 / 10, 0, 7);
    H->photo.filter = (uint8_t)(opt_get(OPT_CAMERA_FILTER) % 5);
    H->sticker_sel = 0;
    H->mode = HM_PHOTO;
    H->held = -1;
}

static void update_photo(void)
{
    if (btn_pressed(BTN_L) || btn_pressed(BTN_R)) {
        H->view_k = (uint8_t)((H->view_k + (btn_pressed(BTN_R) ? 1 : 3)) & 3);
        H->photo.view = H->view_k;
        audio_sfx(SFX_WHOOSH);
    }
    if (btn_repeat(BTN_UP) && H->photo.daylight < 7) H->photo.daylight++;
    if (btn_repeat(BTN_DOWN) && H->photo.daylight > 0) H->photo.daylight--;
    if (btn_repeat(BTN_LEFT) && H->sticker_sel) {
        int s = H->sticker_sel - 1;
        if (H->photo.sx[s] > 0) H->photo.sx[s]--;
    }
    if (btn_repeat(BTN_RIGHT) && H->sticker_sel) {
        int s = H->sticker_sel - 1;
        if (H->photo.sx[s] < 15) H->photo.sx[s]++;
    }
    if (btn_pressed(BTN_SELECT)) {
        H->photo.filter = (uint8_t)((H->photo.filter + 1) % 5);
        camfx_pal_lerp(filter_pal[H->photo.filter], filter_pal[H->photo.filter] ? 8 : 0);
        say(filter_names[H->photo.filter]);
    }
    if (btn_pressed(BTN_B)) {
        /* the next sticker (3 at most): heart, "it's fine.", the horned poop */
        int s = H->sticker_sel < 3 ? H->sticker_sel : 0;
        H->photo.sticker[s] = (uint8_t)(s + 1);
        H->photo.sx[s] = (uint8_t)(4 + s * 4);
        H->photo.sy[s] = (uint8_t)(2 + s);
        H->sticker_sel = (uint8_t)(s + 1);
        audio_sfx_synth(SFX_POP2);
    }
    if (btn_pressed(BTN_A)) {
        int slot;
        H->photo.hash = home_room_hash(&H->ed.room);
        slot = home_photo_add(&H->photo);
        H->shot_age = 0;
        audio_sfx_synth(SFX_TIK);
        say(slot >= 0 ? "Snap! It's in the gallery." : "The album is full (16).");
    }
    if (btn_pressed(BTN_START)) {
        H->mode = H->kind == HK_VISIT ? HM_LIVE : HM_BUILD;
        camfx_pal_lerp(PAL_LERP_NONE, 0);
    }
}

static void update_view(void)
{
    if (btn_pressed(BTN_L) || btn_pressed(BTN_R)) {
        int d = btn_pressed(BTN_R) ? 1 : HOME_PHOTOS - 1, k;
        HomePhoto p;
        for (k = 1; k <= HOME_PHOTOS; k++) {
            int s = (H->view_slot + d * k) % HOME_PHOTOS;
            if (home_photo_get(s, &p)) {
                int arg = HOME_ARG_STORY(HSM_PHOTO) | (s << 8);
                home_enter(arg);
                return;
            }
        }
    }
    if (btn_pressed(BTN_B) || btn_pressed(BTN_A)) leave_now();
}

/* ---- Hold the Door ------------------------------------------------------------------------ */
static void update_defend(void)
{
    HomeDefend *d = &H->def;
    int dx, dy, running;
    if (dpad(&dx, &dy)) {
        int nx = H->cx + dx, ny = H->cy + dy;
        if (nx >= 0 && ny >= 0 && nx < room_w() && ny < room_d()) { H->cx = (int8_t)nx; H->cy = (int8_t)ny; audio_sfx(SFX_CURSOR); }
    }
    if (btn_pressed(BTN_A) && d->phase < HDP_DAWN) {
        int idx = 0, a = home_defend_action_at(d, H->cx, H->cy, &idx);
        if (a >= 0 && home_defend_act(d, a, idx)) audio_sfx(a == HDA_REWIND ? SFX_WELD : SFX_SELECT);
        else nope(0);
    }
    running = home_defend_tick(d);
    if (d->ev & HDE_DOOR_HIT) { audio_sfx(SFX_THUD); camfx_punch(0, 1); }
    if (d->ev & HDE_RIMSHOT) audio_sfx_synth(SFX_TINK);
    if (d->ev & HDE_SHOT) audio_sfx(SFX_PISTOL);
    if (d->ev & HDE_KILL) audio_sfx_synth(SFX_POP2);
    if (d->ev & (HDE_PLATE_HIT | HDE_WINDOW_BREAK)) camfx_shake(SHK_TAP);
    if (d->ev & HDE_DOOR_BREAK) { camfx_shake(SHK_IMPACT); audio_sfx(SFX_EXPLODE); say("The door gave!"); }
    if (d->ev & HDE_ALARM) { camfx_shake(SHK_TAP); say(d->alarm >= 3 ? "...it held." : "The door is going to give!"); }
    if (d->ev & HDE_CHIMNEY) { audio_music_mute(ht[HT_FREEZE_SIL]); say("Something is in the chimney."); }
    if (d->ev & HDE_WAVE) { audio_sfx(SFX_DEMON_ROAR); say(d->wave + 1 >= d->nwaves ? "The last wave!" : "Here they come!"); }
    if (d->ev & HDE_SCARE) audio_sfx(SFX_SAD);
    if (d->ev & HDE_DAWN) { audio_sfx_synth(SFX_FANFARE); camfx_pal_lerp_to(PAL_SEPIA, 4, ht[HT_DAWN_CARD] / 3); }
    if (!running && !H->def_card) {
        home_defend_finish(d);
        H->def_card = 1;
        H->done = 1;
        H->hn_age = 0;
        home_refresh_cache(&H->ed.room, H->room);
    }
    if (H->def_card && btn_pressed(BTN_A | BTN_B)) leave_now();
}

/* ---- the story goals ------------------------------------------------------------------------ */
static int tutorial_has(int role, int furn)
{
    int i;
    for (i = 0; i < H->ed.room.nobj; i++) {
        const DoomoFurn *f = home_furn(HO_FURN(&H->ed.room.obj[i]));
        if (!f) continue;
        if (furn >= 0 ? HO_FURN(&H->ed.room.obj[i]) == furn : f->role == role) return 1;
    }
    return 0;
}

static int goal_met(void)
{
    rescore();
    switch (H->story) {
    case HSM_TUTORIAL:
        return tutorial_has(-1, FURN_PICNIC_CRATE) && tutorial_has(FR_NEST, -1) && tutorial_has(FR_LAMP, -1);
    case HSM_GOAL:
        return H->param > HG_NONE && (H->score.stamps & HOME_STAMP(H->param)) != 0;
    case HSM_DECORATE_FOR:
        return home_layout_progress(&H->ed.room, H->param, -1) >= 8;
    default:
        return 0;
    }
}

static void story_tick(void)
{
    if (H->kind != HK_STORY || H->done) return;
    if ((H->story == HSM_TUTORIAL || H->story == HSM_GOAL || H->story == HSM_DECORATE_FOR) && goal_met()) {
        H->done = 1;
        H->hn_age = 0;
        H->sc_age = 0;
        H->sc_grade = (uint8_t)home_stamps_grade(H->score.stamps);
        if (H->sc_grade == HG_NONE) H->sc_grade = 0xFD;               /* "DONE!" */
        if (H->story == HSM_TUTORIAL)                   /* a raw bit: no row, no toast (home_int.h) */
            doomo_town.unlocks[HUL_RAW_TUTORIAL >> 3] |= (uint8_t)(1 << (HUL_RAW_TUTORIAL & 7));
        commit();
        audio_sfx_synth(SFX_FANFARE);
    }
}

/* the haunting's night beats: lights out, the insert, the 3 AM move */
static void haunt_tick(void)
{
    if (H->hn == HN_NONE) return;
    H->hn_age++;
    if (H->hn == HN_REVEAL && H->hn_age > ht[HT_MSG]) { H->hn = H->story == HSM_HAUNT_NIGHT ? HN_LIGHTS : HN_NONE; H->hn_age = 0; }
    else if (H->hn == HN_LIGHTS) {
        if (H->hn_age == ht[HT_LIGHTS_HOLD]) camfx_fade_to(16, ht[HT_LIGHTS_FADE]);
        if (H->hn_age == ht[HT_LIGHTS_HOLD] + ht[HT_LIGHTS_FADE]) {
            camfx_flash(PAL_UI_RED, 10, 1);
            audio_sfx_synth(SFX_GLITCH);
        }
        if (H->hn_age > ht[HT_LIGHTS_HOLD] + ht[HT_LIGHTS_FADE] + 1) {
            int i;
            camfx_fade_to(0, 0);
            for (i = 0; i < H->ed.room.nobj; i++) {
                const DoomoFurn *f = home_furn(HO_FURN(&H->ed.room.obj[i]));
                if (f && (f->flags & FF_LIGHT)) H->ed.room.obj[i].pa |= HOS_OFF;
            }
            H->ed.gen++;
            H->hn = HN_INSERT;
            H->hn_age = 0;
            H->ins_age = 0;
            audio_music_mute(ht[HT_INSERT]);
            camfx_shake(SHK_IMPACT);
        }
    } else if (H->hn == HN_INSERT && H->hn_age > ht[HT_INSERT] + ht[HT_INSERT_GAP] + ht[HT_FINE_AT]) {
        say(H->pet_on ? "bep?" : "...it's fine.");
        H->hn = HN_AFTER;
        H->hn_age = 0;
    } else if (H->hn == HN_AFTER && H->hn_age > ht[HT_MSG]) {
        H->hn = HN_3AM;
        H->hn_age = 0;
        audio_music_mute(ht[HT_FREEZE_SIL] + 1 + ht[HT_FREEZE_POST]);
    } else if (H->hn == HN_3AM) {
        if (H->hn_age == ht[HT_FREEZE_SIL]) {
            int idx = home_haunt_3am(&H->ed.room, H->room);
            if (idx >= 0) { H->ed.gen++; audio_sfx_synth(SFX_TIK); }
        }
        if (H->hn_age > ht[HT_FREEZE_SIL] + 1 + ht[HT_FREEZE_POST]) { H->hn = HN_NONE; H->done = 1; commit(); }
    }
}

/* the pet: hops between the lamp, the bed's plushie and the nest */
static void pet_tick(void)
{
    if (!H->pet_on) return;
    if (H->pet_age == 0xFF) {
        if ((H->t % (ht[HT_BREATH] * 3)) == 0) {
            int i, n = 0, cand[8];
            for (i = 0; i < H->ed.room.nobj && n < 8; i++) {
                const DoomoFurn *f = home_furn(HO_FURN(&H->ed.room.obj[i]));
                if (f && (f->role == FR_LAMP || f->role == FR_PLUSH || f->role == FR_NEST || f->role == FR_BED)) cand[n++] = i;
            }
            if (n) { H->pet_from = H->pet_to; H->pet_to = (uint8_t)cand[(H->t / 7) % n]; H->pet_age = 0; }
        }
    } else if (++H->pet_age >= ht[HT_HOP] * 3) {
        H->pet_age = 0xFF;
    }
}

/* the open house: three visitors arrive 8 frames apart, rate 15 apart */
static void open_house_tick(void)
{
    if (H->kind != HK_STORY || H->story != HSM_OPEN_HOUSE || H->done) return;
    H->oh_age++;
    if (H->oh_age == ht[HT_ARRIVE_GAP] * 3 + ht[HT_HOP] * 4) {
        rescore();
        H->react = HVR_NOD;
        H->react_actor = AC_VISITOR0;
        H->react_age = 0;
        H->react_line = "Nice.";
    } else if (H->oh_age == ht[HT_ARRIVE_GAP] * 3 + ht[HT_HOP] * 4 + ht[HT_RATE_GAP]) {
        H->react = HVR_HEARTS;
        H->react_actor = AC_VISITOR0 + 1;
        H->react_age = 0;
        H->react_line = "!";
    } else if (H->oh_age == ht[HT_ARRIVE_GAP] * 3 + ht[HT_HOP] * 4 + ht[HT_RATE_GAP] * 2) {
        H->react = HVR_DOUBLE_TAKE;
        H->react_actor = AC_VISITOR0 + 2;
        H->react_age = 0;
        H->react_line = "...it's perfect.";
        H->sc_age = 0;
        rescore();
        H->sc_grade = (uint8_t)home_stamps_grade(H->score.stamps);
        if (H->sc_grade == HG_NONE) H->sc_grade = 0xFD;
        H->done = 1;
        commit();
    }
}

/* ---- the menus --------------------------------------------------------------------------- */
static int list_nav(int rows)
{
    if (H->nrows <= 0) return 0;
    if (btn_repeat(BTN_UP)) { H->sel = (int16_t)((H->sel + H->nrows - 1) % H->nrows); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { H->sel = (int16_t)((H->sel + 1) % H->nrows); audio_sfx(SFX_CURSOR); }
    if (H->sel < H->top) H->top = H->sel;
    if (H->sel >= H->top + rows) H->top = (int16_t)(H->sel - rows + 1);
    return 1;
}

static void main_pick(int item)
{
    H->menu = HMN_NONE;
    switch (item) {
    case MI_CATALOG: case MI_GIFT:
        open_menu(HMN_CATALOG);
        if (!H->nrows) { H->menu = HMN_NONE; say("Storage is empty."); }
        else H->gift_mode = (uint8_t)(item == MI_GIFT);
        break;
    case MI_MODE:
        H->mode = H->mode == HM_BUILD ? HM_LIVE : HM_BUILD;
        H->held = -1;
        break;
    case MI_SCORE: open_menu(HMN_SCORE); break;
    case MI_WALLS: open_menu(HMN_WALLS); break;
    case MI_THEMES: open_menu(HMN_THEMES); break;
    case MI_BLUEPRINTS: open_menu(HMN_BLUEPRINTS); break;
    case MI_ROOMS: open_menu(HMN_ROOMS); break;
    case MI_PHOTO: photo_begin(); break;
    default: leave_now(); break;
    }
}

static void update_menu(void)
{
    int rows = plat_screen_h() >= 200 ? 9 : 6;
    H->a_hold = 0xFFFF;                                  /* the A that closes a menu never picks up */
    if (H->menu == HMN_SCORE) {
        if (H->sc_age != 0xFF && H->sc_age < 0xF0) H->sc_age++;
        if (btn_pressed(BTN_A | BTN_B | BTN_START)) { H->menu = HMN_NONE; audio_sfx(SFX_CANCEL); }
        return;
    }
    if (btn_pressed(BTN_B)) { H->menu = HMN_NONE; audio_sfx(SFX_CANCEL); return; }
    if (H->menu == HMN_CATALOG && H->ncat_tabs > 1 && (btn_pressed(BTN_L) || btn_pressed(BTN_R))) {
        H->cat_tab = (uint8_t)((H->cat_tab + H->ncat_tabs + (btn_pressed(BTN_R) ? 1 : -1)) % H->ncat_tabs);
        H->sel = H->top = 0;
        build_catalog();
        audio_sfx(SFX_BLIP);
        return;
    }
    if (H->menu == HMN_WALLS) {
        static const uint8_t vmax[4] = { 8, 32, 8, 32 };
        if (btn_repeat(BTN_UP)) { H->wall_row = (uint8_t)((H->wall_row + 4) % 5); audio_sfx(SFX_CURSOR); }
        if (btn_repeat(BTN_DOWN)) { H->wall_row = (uint8_t)((H->wall_row + 1) % 5); audio_sfx(SFX_CURSOR); }
        if (H->wall_row < 4 && (btn_repeat(BTN_LEFT) || btn_repeat(BTN_RIGHT))) {
            int m = vmax[H->wall_row];
            H->wall_v[H->wall_row] = (uint8_t)((H->wall_v[H->wall_row] + (btn_repeat(BTN_RIGHT) ? 1 : m - 1)) % m);
            audio_sfx(SFX_CURSOR);
        }
        if (btn_pressed(BTN_A)) {
            memcpy(H->cut_v, H->wall_v, 3);
            H->menu = HMN_NONE;
            start_cut(CUT_WALLS, 0);
        }
        return;
    }
    list_nav(H->menu == HMN_MAIN ? H->nrows : rows);
    if (!btn_pressed(BTN_A) || H->nrows <= 0) return;
    switch (H->menu) {
    case HMN_MAIN: main_pick(H->mi[H->sel]); break;
    case HMN_CATALOG:
        hand_from_catalog(H->rows[H->sel]);
        if (H->gift_mode) H->mode = HM_BUILD;
        H->menu = HMN_NONE;
        break;
    case HMN_THEMES:
        H->menu = HMN_NONE;
        start_cut(CUT_THEME, H->sel);
        break;
    case HMN_BLUEPRINTS: {
        int r = H->rows[H->sel];
        H->menu = HMN_NONE;
        if (r & 0x8000) {
            say(home_blueprint_save(r & 1, &H->ed.room) ? "Saved as your blueprint." : "Nothing to save.");
        } else {
            home_edit_blueprint(&H->ed, r, &H->plan);
            {
                uint8_t ng = H->plan.nghost;
                clear_fx();                                /* clear_fx forgets the ghosts */
                H->plan.nghost = ng;
            }
            montage_start();
        }
        break;
    }
    case HMN_ROOMS:
        H->menu = HMN_NONE;
        if (H->rows[H->sel] != H->room) start_cut(CUT_ROOM, H->rows[H->sel]);
        break;
    case HMN_GROUP:
        H->menu = HMN_NONE;
        if (H->sel == GI_COPY) { home_edit_copy(&H->ed, H->sel_idx, H->nsel, &H->clip); say("Copied."); }
        else if (H->sel == GI_PASTE) {
            int n = home_edit_paste(&H->ed, &H->clip, H->cx, H->cy), i;
            for (i = 0; i < n; i++) fx_drop(H->ed.room.nobj - n + i, i * ht[HT_GROUP_GAP]);
            if (!n) nope(HPR_NOT_OWNED);
            H->nsel = 0;
        } else if (H->sel == GI_STORE) {
            home_edit_group_remove(&H->ed, H->sel_idx, H->nsel);
            memset(H->oa_k, 0, sizeof(H->oa_k));
            H->nsel = 0;
            audio_sfx_synth(SFX_SHWIP);
        } else {
            H->nsel = 0;
        }
        break;
    default: H->menu = HMN_NONE; break;
    }
}

/* ---- update ------------------------------------------------------------------------------ */
static void reclaim(void)
{
    /* another scene took the scratch (HomeMart, a dialogue that claims it):
     * enter again from the remembered arg; the undo history is gone */
    if (resume_arg_set && !scene_scratch_mine(HD_TAG)) home_enter(resume_arg);
}

static void home_update(void)
{
    reclaim();
    if (!H) { scene_pop(); return; }
    plat_code_part(H->part);                    /* the mode's code (scene.h): first */
    camfx_tick();
    H->t++;
    if (H->msg_age != 0xFFFF && ++H->msg_age > ht[HT_MSG]) H->msg_age = 0xFFFF;
    if (H->nope_age != 0xFF && ++H->nope_age >= ht[HT_NOPE]) H->nope_age = 0xFF;
    if (H->flip_age != 0xFF && ++H->flip_age >= ht[HT_FLIP]) H->flip_age = 0xFF;
    if (H->lift_age != 0xFF && ++H->lift_age > 2 + ht[HT_LIFT]) H->lift_age = 0xFF;
    if (H->undo_age != 0xFF && ++H->undo_age > ht[HT_UNDO_STRETCH] + ht[HT_UNDO_POP]) H->undo_age = 0xFF;
    if (H->shot_age != 0xFF && ++H->shot_age > ht[HT_SHUTTER] + ht[HT_HUD_POP]) H->shot_age = 0xFF;
    if (H->ins_age != 0xFF && ++H->ins_age > ht[HT_INSERT] + ht[HT_INSERT_GAP] + ht[HT_FINE_AT]) {
        H->ins_age = 0xFF;
        if (H->hn == HN_NONE) {
            /* one persistent change: a frame tilted (a wall piece turns) */
            int i;
            for (i = 0; i < H->ed.room.nobj; i++) {
                const DoomoFurn *f = home_furn(HO_FURN(&H->ed.room.obj[i]));
                if (f && (f->flags & FF_WALL) && !(f->flags2 & FF_PORTAL) && !home_obj_is_fixed(&H->ed.room, i)) {
                    H->ed.room.obj[i].pb = (uint8_t)((H->ed.room.obj[i].pb & 0x1F) | HOS_OPEN);
                    break;
                }
            }
            say("...it's fine.");
            audio_music_resume_on_beat();
        }
    }
    if (H->exp_age != 0xFF && H->exp_age < 0xF0) {
        H->exp_age++;
        if (H->exp_age > ht[HT_RUMBLE] + ht[HT_RUMBLE_QUIET] &&
            (H->exp_age - ht[HT_RUMBLE] - ht[HT_RUMBLE_QUIET]) % ht[HT_TILE_GAP] == 0 &&
            (H->exp_age - ht[HT_RUMBLE] - ht[HT_RUMBLE_QUIET]) / ht[HT_TILE_GAP] <= room_w() + room_d())
            audio_sfx(SFX_THUD);
        if ((H->exp_age - ht[HT_RUMBLE] - ht[HT_RUMBLE_QUIET]) / ht[HT_TILE_GAP] == room_w() + room_d() + 1 &&
            (H->exp_age - ht[HT_RUMBLE] - ht[HT_RUMBLE_QUIET]) % ht[HT_TILE_GAP] == 0) {
            audio_sfx_synth(SFX_FANFARE);
            H->exp_age = 0xFF;
        }
    }
    if (H->react_age != 0xFF) {
        if (H->react_age == 0) {
            if (H->react == HVR_DOUBLE_TAKE) audio_sfx_synth(SFX_TIK);
            if (H->react == HVR_HEARTS) audio_sfx(SFX_LOVE);
            if (H->react == HVR_BANG) audio_sfx(SFX_HAPPY);
            if (H->react == HVR_FLEE) audio_sfx(SFX_SAD);
            if (H->react == HVR_EW) audio_sfx(SFX_SQUEAK);
            audio_music_duck(H->react == HVR_DEADPAN ? 128 : 154, ht[HT_REACT_BUBBLE]);
            if (H->pet_on && !H->pet_beeped && H->react == HVR_DEADPAN) { H->pet_beeped = 1; audio_sfx_synth(SFX_BEEP); }
        }
        if (++H->react_age > ht[HT_REACT_LOOK] + ht[HT_REACT_SHOCK] + ht[HT_REACT_STARE] + ht[HT_REACT_BUBBLE]) {
            if (H->react == HVR_FLEE && H->react_actor == AC_HOST) H->host_out = 1;   /* out of the door */
            H->react_age = 0xFF;
        }
    }
    if (H->react_cool) H->react_cool--;
    if (H->sleep_age != 0xFF) {
        H->sleep_age++;
        if (H->sleep_age == ht[HT_SLEEP_FADE] + ht[HT_SLEEP_MOON]) camfx_fade_to(16, 0);
        if (H->sleep_age > ht[HT_SLEEP_FADE] + ht[HT_SLEEP_MOON] + ht[HT_SLEEP_BLACK]) {
            camfx_fade_to(0, ht[HT_IRIS] * 2);
            H->sleep_age = 0xFF;
            say("Good morning.");
        }
        return;                                          /* no button in the heartfelt beat */
    }
    if (H->sc_age != 0xFF && H->menu != HMN_SCORE && H->sc_age < 0xF0) {
        H->sc_age++;
        if (H->sc_age == 2) camfx_shake(SHK_STAMP);
        if (H->done && H->kind == HK_STORY && H->sc_age >= ht[HT_GOAL_HOLD] && H->story != HSM_DEFEND) {
            H->sc_age = 0xFF;
            if (H->story != HSM_OPEN_HOUSE || btn_pressed(BTN_A)) leave_now();
            return;
        }
        if (H->sc_age > ht[HT_STAMP] * 3 && !(H->done && H->kind == HK_STORY)) H->sc_age = 0xFF;
    }
    fx_tick();
    cut_tick();
    montage_tick();
    haunt_tick();
    pet_tick();
    open_house_tick();
    if (H->brake_age != 0xFF) {
        H->brake_age++;
        if (H->brake_age == ht[HT_BRAKE]) {
            H->view_k = (uint8_t)((H->view_k + H->brake_dir) & 3);
            audio_sfx(SFX_THUD);
        }
        if (H->brake_age > ht[HT_BRAKE]) H->brake_age = 0xFF;
    }
    {
        /* the cursor glides in view space */
        int x0 = H->cx, y0 = H->cy, x1 = x0 + 1, y1 = y0 + 1, i, w = room_w(), d = room_d(), t;
        for (i = 0; i < H->view_k; i++) {
            int nx0 = w - y1, nx1 = w - y0, ny0 = x0, ny1 = x1;
            x0 = nx0; x1 = nx1; y0 = ny0; y1 = ny1;
            t = w; w = d; d = t;
        }
        tw_spring_target(&H->cur_x, (x0 * 16) * 16);
        tw_spring_target(&H->cur_y, (y0 * 16) * 16);
        tw_spring_step(&H->cur_x);
        tw_spring_step(&H->cur_y);
    }
    if (H->cut != CUT_NONE || H->hn == HN_LIGHTS || H->hn == HN_INSERT || H->hn == HN_3AM) return;
    if (H->mode == HM_VIEW) { update_view(); return; }
    if (H->mode == HM_DEFEND) { update_defend(); return; }
    story_tick();
    if (H->menu != HMN_NONE) { update_menu(); return; }
    if (H->mode == HM_PHOTO) { update_photo(); return; }
    if (btn_pressed(BTN_START)) { open_menu(HMN_MAIN); return; }
    if (H->mode == HM_BUILD) update_build();
    else update_live();
}

/* ---- drawing ------------------------------------------------------------------------------ */
static int anim_frame(const DoomoFurn *f, int furn, int idx)
{
    int frames = f->frames ? f->frames : 1, t = H->t + idx * 9;
    if (!(f->flags & FF_ANIM) || frames <= 1) return 0;
    if (H->kind == HK_VISIT && H->visit.kind == HVK_SEALED && f->role != FR_CANDLE) return 0;
    if (H->hn == HN_3AM && H->hn_age < ht[HT_FREEZE_SIL] + 1 + ht[HT_FREEZE_POST]) return 0;   /* frozen */
    if (furn == FURN_PLAIN_HUMAN) return 0;
    if (furn == FURN_KEVINS_BUST) return (t % ht[HT_BLINK_EVERY]) < ht[HT_BLINK_LEN];
    if (furn == FURN_HEART_WALLPAPER_EYE) return home_haunt_eye_blinks() && (t % ht[HT_BLINK_EVERY]) < ht[HT_BLINK_LEN];
    if (furn == FURN_DAYS_SINCE_FIRE) return home_days_since_fire() == 0 ? (t >> 2) & 1 : 0;
    if (f->interact == FI_WATCH) return (t / ht[HT_TV]) % frames;
    if (furn == FURN_SUMMER_FAN) return (t / ht[HT_FAN]) % frames;
    if (furn == FURN_WEEPING_FISH_TANK) return (t % ht[HT_BUBBLES]) < ht[HT_BLINK_LEN];
    if (f->role == FR_CANDLE) {
        int k = (t / (candle_gaps[0] + candle_gaps[1] + candle_gaps[2])) % 3, r = t % 15;
        return (r < candle_gaps[0] ? 0 : r < candle_gaps[0] + candle_gaps[1] ? 1 : 2 + k) % frames;
    }
    return (t >> 3) % frames;
}

static void obj_fx(DoomoRoomObj *o, int idx)
{
    const DoomoFurn *f = home_furn(o->furn);
    int k = idx < HOME_OBJ_MAX ? H->oa_k[idx] : 0, a = idx < HOME_OBJ_MAX ? H->oa_t[idx] : 0;
    if (f) o->rot_anim = RO_ROT_ANIM(RO_ROT(o), anim_frame(f, o->furn, idx));
    if (H->kind == HK_VISIT && H->visit.kind == HVK_POSSESSED) o->jitter = 2;
    if (H->ed.room.obj[idx].pb & HOS_OPEN && f && (f->flags & FF_WALL)) o->sq_x = -6;   /* tilted */
    if (!k || (f && (f->flags2 & FF_NO_SQUASH) && k != OA_HIDDEN)) return;
    if (k == OA_DROP) {
        if (a < 0) { o->flags |= RO_GHOST; o->z = (uint8_t)(o->z + DROP_FROM_PX + 40); return; }
        if (a < ht[HT_DROP_FALL]) {
            fix16 t = tw_t(a, ht[HT_DROP_FALL]);
            int fall = DROP_FROM_PX - tw_lerp(0, DROP_FROM_PX, tw_in2(t));
            o->z = (uint8_t)(o->z + fall);
            o->sq_x = (int8_t)((218 - 256) / 2);          /* stretched 0.85 / 1.18 */
            o->sq_y = (int8_t)((302 - 256) / 2);
        } else {
            TwSquash s = tw_squash(&TW_DROP_SOFT_SX, &TW_DROP_SOFT_SY, &TW_DROP_SOFT_DY, a - ht[HT_DROP_FALL],
                                   tw_bounce_amount());
            o->sq_x = (int8_t)((s.sx - 256) / 2);
            o->sq_y = (int8_t)((s.sy - 256) / 2);
            if (s.dy < 0 && o->z < (uint8_t)(-s.dy)) o->z = 0; else o->z = (uint8_t)(o->z - s.dy);
        }
    } else if (k == OA_RIPPLE_NEAR || k == OA_RIPPLE_FAR) {
        int delay = k == OA_RIPPLE_NEAR ? ht[HT_RIPPLE_NEAR] : ht[HT_RIPPLE_FAR];
        int v = H->rip_hist[(H->rip_t - 1 - delay) & 7];   /* Q4 px */
        int amp = k == OA_RIPPLE_NEAR ? 102 : 38;          /* 0.4 / 0.15 of 256 */
        int s = (v * amp / 256) * tw_bounce_amount() / 256;
        if (a >= delay) { o->sq_x = (int8_t)DOOMO_CLAMP(s / 8, -40, 40); o->sq_y = (int8_t)DOOMO_CLAMP(-s / 8, -40, 40); }
    } else if (k == OA_SPLAT) {
        o->flags |= RO_GHOST;                              /* hidden behind the splat */
    } else if (k == OA_HIDDEN) {
        o->flags |= RO_GHOST;
        o->z = (uint8_t)(o->z + 200);                      /* off the top: not yet landed */
    }
}

static int build_view(DoomoRoomView *v, uint8_t *cache, uint32_t cap)
{
    const HomeRoom *r = &H->ed.room;
    const HomeRoomDef *d = &home_room_defs[H->room];
    int i, n = 0;
    memset(v, 0, sizeof(*v));
    v->w = (uint8_t)room_w();
    v->d = (uint8_t)room_d();
    v->wallpaper = r->wallpaper == 0xFF ? d->wallpaper : r->wallpaper;
    v->floor = r->floor == 0xFF ? d->floor : r->floor;
    v->wall_paint = r->wall_paint == 0xFF ? d->wall_paint : r->wall_paint;
    v->floor_paint = r->floor_paint == 0xFF ? d->floor_paint : r->floor_paint;
    v->ceiling = (uint8_t)((r->flags & HRF_CEIL_MASK) >> HRF_CEIL_SHIFT);
    v->room_kind = d->kind;
    for (i = 0; i < r->nobj && n < HOME_OBJ_MAX; i++) {
        home_obj_view(&r->obj[i], &H->objs[n]);
        obj_fx(&H->objs[n], i);
        if ((H->objs[n].flags & RO_GHOST) && H->objs[n].z > 150) continue;   /* hidden: skip */
        n++;
    }
    /* the blueprint's grey ghosts, once the montage is down */
    if (H->mont_n == 0)
        for (i = 0; i < H->plan.nghost && i < HD_GHOSTS && n < HOME_OBJ_MAX + HD_GHOSTS; i++) {
            const HomeGhost *g = &H->plan.ghost[i];
            DoomoRoomObj *o = &H->objs[n++];
            memset(o, 0, sizeof(*o));
            o->furn = g->furn;
            o->x = g->x; o->y = g->y;
            o->rot_anim = RO_ROT_ANIM(g->rot, 0);
            o->paint_a = o->paint_b = 0xFF;
            o->flags = RO_GHOST | RO_SEMI;
            {
                const DoomoFurn *f = home_furn(g->furn);
                if (f && (f->flags & FF_WALL)) o->z = 24;
            }
        }
    /* the selection blinks */
    for (i = 0; i < H->nsel; i++)
        if (H->sel_idx[i] < n) H->objs[H->sel_idx[i]].flags |= RO_SELECTED;
    if (H->held >= 0 && H->mode == HM_BUILD && n < HOME_OBJ_MAX + HD_GHOSTS) {
        DoomoRoomObj *g = &H->objs[n];
        const DoomoFurn *f = home_furn(H->held);
        HomeFit fit;
        int ok = home_fit_sub(r, H->room, H->held, H->cx, H->cy, H->held_rot, H->held_sub, -1, &fit) == HPR_OK;
        int bob = 2 + ui_wave(H->t, ht[HT_GHOST_PERIOD], 2);
        memset(g, 0, sizeof(*g));
        g->furn = (uint16_t)H->held;
        g->x = (uint8_t)H->cx;
        g->y = (uint8_t)H->cy;
        g->z = (uint8_t)((ok ? fit.z * 4 : (f && (f->flags & FF_WALL) ? 24 : 0)) + LIFT_PX + bob);
        g->rot_anim = RO_ROT_ANIM(H->held_rot, 0);
        g->paint_a = f ? (H->held_pa == FURN_PAINT_DEFAULT ? f->paint_a : H->held_pa) : 0xFF;
        g->paint_b = f ? (H->held_pb == FURN_PAINT_DEFAULT ? f->paint_b : H->held_pb) : 0xFF;
        g->flags = (uint8_t)(RO_GHOST | (ok ? 0 : RO_INVALID));
        if (H->nope_age != 0xFF) {
            g->jitter = (int8_t)(nope_dx[H->nope_age & 3] * 2);
            g->sq_x = -8; g->sq_y = 8;                     /* the sad 0.94 / 1.06 */
        }
        if (H->lift_age != 0xFF) {
            if (H->lift_age < 2) { g->sq_x = 13; g->sq_y = -13; }        /* 1.10 / 0.90 */
            else if (H->lift_age < 2 + ht[HT_LIFT]) { g->sq_x = -10; g->sq_y = 13; }
        }
        if (H->flip_age != 0xFF) {
            int sx = flip_sx[H->flip_age < 6 ? H->flip_age : 5];
            if (sx < 0) sx = -sx;
            g->sq_x = (int8_t)((sx < 2 ? 2 - 256 : sx - 256) / 2);
            if (H->flip_age < ht[HT_FLIP_SWAP]) g->rot_anim = RO_ROT_ANIM(H->held_rot - 1, 0);
        }
        n++;
    }
    /* the undo's reverse DROP */
    if (H->undo_age != 0xFF && H->undo_obj.furn && n < HOME_OBJ_MAX + HD_GHOSTS) {
        DoomoRoomObj *u = &H->objs[n++];
        *u = H->undo_obj;
        if (H->undo_age < ht[HT_UNDO_STRETCH]) { u->sq_x = -10; u->sq_y = 13; }
        else {
            fix16 t = tw_t(H->undo_age - ht[HT_UNDO_STRETCH], ht[HT_UNDO_POP]);
            fix16 inv = FIX_ONE - t;
            u->z = (uint8_t)(u->z + UNDO_RISE_PX - tw_lerp(0, UNDO_RISE_PX, (fix16)((int64_t)inv * inv / FIX_ONE)));
            u->flags |= RO_GHOST;
        }
    }
    v->objs = H->objs;
    v->nobj = n;
    v->gen = H->ed.gen;
    v->yaw = (int16_t)(H->view_k * 1024);
    v->punch_q8 = 256;
    v->daylight = (uint8_t)light_level();
    if (H->kind == HK_VISIT && H->visit.tint) { v->tint = H->visit.tint; v->tint_level = H->visit.tint == TINT_RED ? 4 : 8; }
    if (H->ins_age != 0xFF && H->ins_age < ht[HT_INSERT] && !opt_get(OPT_REDUCE_FLASH)) { v->tint = TINT_RED; v->tint_level = 16; }
    v->cursor_x = (int8_t)((H->mode == HM_BUILD || H->mode == HM_DEFEND) && H->menu == HMN_NONE ? H->cx : -1);
    v->cursor_y = (int8_t)((H->mode == HM_BUILD || H->mode == HM_DEFEND) && H->menu == HMN_NONE ? H->cy : -1);
    if (H->band) band_mask(v);
    v->time = H->t;
    v->vx = 0;
    v->vy = 12;
    v->vw = (int16_t)plat_screen_w();
    v->vh = (int16_t)(plat_screen_h() - 24);
    if (H->brake_age != 0xFF && H->brake_age < ht[HT_BRAKE_SHAKE]) v->pan_x = (int8_t)(H->brake_dir * 2);
    v->plat_cache = cache;
    v->plat_cache_cap = cap;
    return n;
}

/* the actors: the player, a host, the pet, the runner, visitors, guests, demons */
static DoomoRoomActor *add_actor(const DoomoImage *img, fix16 x, fix16 y, int z)
{
    DoomoRoomActor *a;
    if (!img || H->nact >= AC_MAX) return NULL;
    a = &H->act[H->nact++];
    memset(a, 0, sizeof(*a));
    a->img = img;
    a->x = x;
    a->y = y;
    a->z = (int16_t)z;
    a->sx = a->sy = 256;
    return a;
}

static int hop_z(int age)
{
    int a = age % ht[HT_HOP], h = ht[HT_HOP];
    return HOP_PX * 4 * a * (h - a) / (h * h);
}

static void build_actors(DoomoRoomView *v)
{
    DoomoRoomActor *a;
    int breath = ui_wave(H->t, ht[HT_BREATH], 1) > 0;
    H->nact = 0;
    if (H->mode == HM_VIEW) { v->actors = H->act; v->nact = 0; return; }
    /* the player: Live, visits, the defence, Build (standing by the door) */
    if (H->mode != HM_PHOTO || H->kind == HK_VISIT) {
        DoomoResident *me = town_res(DOOMO_PLAYER_ID);
        if (me && me->status != RES_EMPTY) {
            int frame = H->pwalk ? (H->pwalk / ht[HT_WALK_FRAME]) & 3 : 0;
            a = add_actor(bodyspr_image(DOOMO_PLAYER_ID, H->pdir, frame), H->px, H->py, H->sitting ? 4 : 0);
            if (a) {
                a->flags = RA_PLAYER;
                if (H->sitting) { a->sx = 270; a->sy = 236; }
                else if (!H->pwalk && breath) a->sy = 262;
            }
        }
    }
    /* the host of a visit */
    if (H->kind == HK_VISIT && !H->host_out) {
        int dir = H->hdir, jit = 0, z = 0, age = H->react_actor == AC_HOST ? H->react_age : 0xFF;
        if (age != 0xFF && H->react == HVR_DOUBLE_TAKE) {
            if (age < ht[HT_REACT_LOOK]) dir = 1;
            else if (age < ht[HT_REACT_LOOK] + ht[HT_REACT_SHOCK]) jit = (age & 1) ? FIX_ONE / 16 : -FIX_ONE / 16;
            else if (age < ht[HT_REACT_LOOK] + ht[HT_REACT_SHOCK] + ht[HT_REACT_STARE]) dir = 0;   /* at the lens */
        } else if (age != 0xFF && (H->react == HVR_HEARTS || H->react == HVR_BANG)) {
            z = hop_z(age) / 2;
        } else if (age != 0xFF && H->react == HVR_FLEE) {
            int t2 = age > ht[HT_REACT_LOOK] ? age - ht[HT_REACT_LOOK] : 0;
            fix16 run = (fix16)((2 * t2 + t2 * t2 * 35 / 100) * (FIX_ONE / 16));
            H->hy = DOOMO_MAX(FIX_ONE / 4, INT2FIX(3) + FIX_ONE / 2 - run);
            z = hop_z(t2);
        }
        a = add_actor(bodyspr_image(H->visit.id, dir, 0), H->hx + jit, H->hy, z);
        if (a) {
            if (H->visit.kind == HVK_GHOST) a->flags |= BB_TRANSLUCENT;
            if (H->react == HVR_EW && age != 0xFF && age < 12) { a->sx = 282; a->sy = 230; }   /* 1.10 / 0.90 */
            else if (H->react == HVR_DEADPAN || H->visit.kind == HVK_POSSESSED) { /* bob = 0 */ }
            else if (breath) a->sy = 262;
        }
    }
    /* the pet, hopping between its places */
    if (H->pet_on && H->kind != HK_VISIT && H->ed.room.nobj) {
        int to = H->pet_to < H->ed.room.nobj ? H->pet_to : 0, from = H->pet_from < H->ed.room.nobj ? H->pet_from : to;
        fix16 x0 = INT2FIX(HO_X(&H->ed.room.obj[from])) + FIX_ONE / 2, y0 = INT2FIX(HO_Y(&H->ed.room.obj[from])) + FIX_ONE;
        fix16 x1 = INT2FIX(HO_X(&H->ed.room.obj[to])) + FIX_ONE / 2, y1 = INT2FIX(HO_Y(&H->ed.room.obj[to])) + FIX_ONE;
        int z = 0;
        if (H->pet_age != 0xFF) {
            fix16 t = tw_t(H->pet_age, ht[HT_HOP] * 3);
            x0 = x0 + (fix16)(((int64_t)(x1 - x0) * t) >> 16);
            y0 = y0 + (fix16)(((int64_t)(y1 - y0) * t) >> 16);
            z = hop_z(H->pet_age);
        } else {
            x0 = x1; y0 = y1;
        }
        if (y0 > INT2FIX(room_d()) - FIX_ONE / 4) y0 = INT2FIX(room_d()) - FIX_ONE / 4;
        a = add_actor(&spr_pet, x0, y0, z);
        if (a && H->pet_age != 0xFF && H->pet_age % ht[HT_HOP] == 0) { a->sx = 312; a->sy = 205; }  /* 1.22 / 0.80 */
    }
    /* the paint-roller runner: x = dir (2a + 0.35 a^2), edge to edge */
    if (H->cut != CUT_NONE && !H->cut_v[3]) {
        int age = H->cut_age;
        fix16 x = (fix16)((2 * age + age * age * 35 / 100) * FIX_ONE * (room_w() + 2) / 140) - FIX_ONE;
        a = add_actor(bodyspr_image(DOOMO_PLAYER_ID, 2, (age >> 1) & 3), x, INT2FIX(room_d()) - FIX_ONE / 3, 0);
    }
    /* the open house: three visitors hop in, 8 frames apart */
    if (H->kind == HK_STORY && H->story == HSM_OPEN_HOUSE) {
        int i;
        for (i = 0; i < H->oh_ids_n; i++) {
            int age = (int)H->oh_age - i * ht[HT_ARRIVE_GAP], hops;
            fix16 y;
            if (age < 0) continue;
            hops = age / ht[HT_HOP];
            y = FIX_ONE / 2 + INT2FIX(hops < 4 ? hops : 4) / 2 + (hops < 4 ? (age % ht[HT_HOP]) * (FIX_ONE / 2) / ht[HT_HOP] : 0);
            a = add_actor(bodyspr_image(H->oh_ids[i], 0, 0), INT2FIX(home_room_defs[H->room].door_at - 1 + i) + FIX_ONE / 2,
                          y + FIX_ONE, hops < 4 ? hop_z(age) : 0);
        }
    }
    /* Hold the Door: the guests and the demons indoors / at the threshold */
    if (H->mode == HM_DEFEND) {
        const HomeDefend *d = &H->def;
        int i, nd = 0;
        for (i = 0; i < HDG_COUNT; i++) {
            const HomeGuest *g = &d->g[i];
            const DoomoImage *img;
            if (!g->present) continue;
            img = home_defend_guest_id[i] < 0 ? &spr_kevin : bodyspr_image(home_defend_guest_id[i], 0, 0);
            a = add_actor(img, INT2FIX(g->x) + FIX_ONE / 2, INT2FIX(g->y) + FIX_ONE / 2, g->flinch ? 2 : 0);
            if (a && !g->safe) a->flags |= BB_TRANSLUCENT;
            if (a && g->flinch) { a->sx = 282; a->sy = 230; }
        }
        for (i = 0; i < HD_DEMONS && nd < 8; i++) {
            const HomeDemon *m = &d->dm[i];
            int lane = m->lane < HDL_COUNT ? m->lane : 0, lx = d->lane_x[lane], ly = d->lane_y[lane], wb;
            fix16 x, y;
            if (m->state == HDS_INSIDE) { x = INT2FIX(m->tx) + FIX_ONE / 2; y = INT2FIX(m->ty) + FIX_ONE / 2; }
            else if ((m->state == HDS_HIT || m->state == HDS_STARE) && m->dist <= 2) {
                wb = d->lane_wall[lane] == HW_B;
                x = INT2FIX(lx) + (wb ? FIX_ONE / 5 : FIX_ONE / 2);
                y = INT2FIX(ly) + (wb ? FIX_ONE / 2 : FIX_ONE / 5);
            } else continue;
            a = add_actor(m->kind == HDK_BRUTE ? &spr_brute : &spr_imp, x, y,
                          m->state == HDS_INSIDE ? hop_z(m->timer) : (m->kind == HDK_BRUTE && lane != HDL_DOOR ? 12 : 0));
            if (a && m->state == HDS_HIT && m->timer < 3) { a->sx = 300; a->sy = 210; }   /* the squash on a hit */
            nd++;
        }
    }
    v->actors = H->act;
    v->nact = H->nact;
}

/* ---- HUD, panels ---------------------------------------------------------------------------- */
/* the bottom bar's line: the full one where it fits, else the brief one */
static void hint(const char *full, const char *brief)
{
    int sw = plat_screen_w();
    ui_text(3, plat_screen_h() - 10, !brief || ui_text_width(full) <= sw - 6 ? full : brief, ui_style(WIN_CUTE)->text);
}

static void draw_hud(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), i, x, pop = 0;
    char buf[48];
    const UiStyle *st = ui_style(WIN_CUTE);
    if (H->shot_age != 0xFF && H->shot_age > ht[HT_SHUTTER]) pop = (tw_tab(&TW_POP_BUBBLE, H->shot_age - ht[HT_SHUTTER]) - 256) / 32;
    plat_rect(0, 0, sw, 12, st->fill);
    plat_rect(0, 11, sw, 1, st->outline);
    plat_rect(0, sh - 12, sw, 12, st->fill);
    plat_rect(0, sh - 12, sw, 1, st->outline);
    plat_room_ui_opaque(0, 0, sw, 12);
    plat_room_ui_opaque(0, sh - 12, sw, 12);
    if (H->kind == HK_VISIT) {
        DoomoResident *r = town_res(H->visit.id);
        int slot = H->visit.id % DOOMO_DISTRICT_SIZE;    /* the facade's numbering: floor-room */
        doomo_snprintf(buf, sizeof(buf), "%s, %s %d-%d", r ? r->name : "?", RES_DISTRICT(H->visit.id) ? "Pits" : "Towers",
                       slot / 10 + 1, slot % 10 + 1);
        ui_text(3, 2 + pop, buf, st->text);
    } else {
        ui_text(3, 2 + pop, home_room_defs[H->room].name, st->text);
    }
    doomo_snprintf(buf, sizeof(buf), "%02d:%02d", doomo_town.minute / 60, doomo_town.minute % 60);
    x = 6 + ui_text_width(home_room_defs[H->room].name) + 4;
    if (H->kind != HK_VISIT) ui_text(x, 2 + pop, buf, st->text_dim);
    rescore();
    x = sw - 4 - 4 * 12;
    for (i = 0; i < 4; i++) {
        int v = H->score.stat[i];
        ui_fill_circle(x + i * 12 + 5, 6 + pop, 4, ui_darker(stat_cols[i]));
        ui_fill_circle(x + i * 12 + 5, 6 + pop, 1 + v * 3 / 100, stat_cols[i]);
    }
    doomo_snprintf(buf, sizeof(buf), "H$%u", (unsigned)doomo_town.money);
    ui_text(x - 4 - ui_text_width(buf), 2 + pop, buf, PAL_UI_GOLD);
    /* the bottom bar: what the buttons do */
    if (H->menu != HMN_NONE) hint(H->menu == HMN_CATALOG ? "A:take  L/R:tab  B:back" :
                                  H->menu == HMN_WALLS ? "Up/Dn:row  L/R:change  A:paint" : "A:choose  B:back", NULL);
    else if (H->mode == HM_DEFEND) hint("A: repair / wind / comfort / plate", "A: repair/wind/comfort/plate");
    else if (H->mode == HM_LIVE) hint(H->kind == HK_VISIT ? "Walk  A:talk/look  START:menu  B:leave" : "Walk  A:use  START:menu  B:leave",
                                      H->kind == HK_VISIT ? "A:talk/look  START:menu  B:leave" : "A:use  START:menu  B:leave");
    else if (H->nsel) hint("Group: move  L/R:turn  START:more  A:done", "L/R:turn  START:more  A:done");
    else if (H->paint_mode) hint("Recolour: L paint A  R paint B", "L: paint A  R: paint B");
    else if (H->held >= 0) {
        const DoomoFurn *f = home_furn(H->held);
        char nm[32];
        doomo_snprintf(buf, sizeof(buf), "%s  A:put  L/R:turn", f ? doomo_furn_text(f->name, nm, sizeof(nm)) : "?");
        hint(buf, "A:put  L/R:turn  B:back");
    } else if (H->plan.nghost && H->mont_n == 0) hint("Grey = Buy at HomeMart  START:menu", "Grey: HomeMart  START:menu");
    else hint("A:pick up (hold: paint)  L/R:view  SELECT:undo  START:menu", "A:pick  L/R:view  SEL:undo");
}

static void draw_caption(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h();
    const UiStyle *st = ui_style(WIN_CUTE);
    if (H->msg_age != 0xFFFF && H->msg) {
        int w = ui_text_width(H->msg) + 10, y = sh - 28;
        int dy = H->msg_age < 4 ? (4 - H->msg_age) * 2 : 0;
        if (w > sw - 8) w = sw - 8;
        ui_round_rect((sw - w) / 2, y + dy, w, 13, 4, st->fill_hi);
        ui_text_center(sw / 2, y + 3 + dy, H->msg, st->text);
    }
}

static void draw_list_rows(int x, int y, int w, int rows, int (*label)(int, char *, int), const UiStyle *st)
{
    int i;
    char buf[48];
    for (i = H->top; i < H->nrows && i < H->top + rows; i++) {
        int ry = y + (i - H->top) * 12, sel = i == H->sel;
        uint8_t c = (uint8_t)label(i, buf, sizeof(buf));
        if (sel) ui_round_rect(x - 2, ry - 1, w + 4, 12, 3, st->fill_hi);
        ui_text(x + (sel ? 3 : 1), ry + 1, buf, c ? c : st->text);
        if (sel) plat_text(x - 1 + ui_wave(H->t, 16, 1), ry + 1, STR_ICON_ARROW_R, st->accent);
    }
    ui_scroll_arrows(x + w - 4, y - 6, y + rows * 12, H->top > 0, H->top + rows < H->nrows);
}

static int label_main(int i, char *buf, int cap)
{
    const char *s = mi_names[H->mi[i]];
    if (H->mi[i] == MI_MODE) s = H->mode == HM_BUILD ? "Live mode" : "Build mode";
    doomo_snprintf(buf, cap, "%s", s);
    return 0;
}

static int label_catalog(int i, char *buf, int cap)
{
    const DoomoFurn *f = home_furn(H->rows[i]);
    char nm[32];
    doomo_snprintf(buf, cap, "%s%s", seen_new(H->rows[i]) ? "NEW! " : "", f ? doomo_furn_text(f->name, nm, sizeof(nm)) : "?");
    return seen_new(H->rows[i]) ? PAL_UI_RED : 0;
}

static int label_theme(int i, char *buf, int cap)
{
    doomo_snprintf(buf, cap, "%s", doomo_home_themes[i].name);
    return 0;
}

static int label_bp(int i, char *buf, int cap)
{
    int r = H->rows[i];
    if (r & 0x8000) { doomo_snprintf(buf, cap, "Save room to slot %d", (r & 1) + 1); return PAL_UI_MINT_D; }
    doomo_snprintf(buf, cap, "%s", home_blueprint_name(r) ? home_blueprint_name(r) : "?");
    return 0;
}

static int label_room(int i, char *buf, int cap)
{
    doomo_snprintf(buf, cap, "%s%s", home_room_defs[H->rows[i]].name, H->rows[i] == H->room ? "  (here)" : "");
    return 0;
}

static int label_group(int i, char *buf, int cap)
{
    doomo_snprintf(buf, cap, "%s", gi_names[i]);
    return i == GI_PASTE && !H->clip.n ? PAL_GRAY : 0;
}

static void draw_sprite_preview(int furn, int px, int py, int pw, int ph, uint8_t *cache, uint32_t cap)
{
    DoomoImage img;
    int ox, oy;
    if (room_ref_sprite(furn, (H->t >> 6) & 1, (H->t >> 3) & 3, -1, -1, 15, cache, cap, &img, &ox, &oy)) {
        /* 2x where the panel has the room for it (the PS1's 320x240 panel) */
        int sc = img.w * 2 + 8 <= pw && img.h * 2 + 8 <= ph ? 2 : 1, bob = ui_wave(H->t, ht[HT_GHOST_PERIOD], 1) * sc;
        plat_image(px + (pw - img.w * sc) / 2, py + (ph - img.h * sc) / 2 + bob, &img, sc, IMG_SHADOW, 0);
    } else {
        room_ref_maquette(furn, -1, -1, px, py, pw, ph);    /* no sprites (the PS1): the prims */
    }
}

static void draw_menu(uint8_t *cache, uint32_t cap)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), rows = sh >= 200 ? 9 : 6;
    const UiStyle *st = ui_style(WIN_CUTE);
    char buf[48];
    if (H->menu == HMN_MAIN || H->menu == HMN_GROUP) {
        int w = 96, n = H->nrows, h = n * 12 + 12;
        if (ui_window_pop(sw - w - 6, 16, w, h, WIN_CUTE, (int)H->t))
            draw_list_rows(sw - w + 2, 22, w - 12, n, H->menu == HMN_MAIN ? label_main : label_group, st);
        return;
    }
    if (H->menu == HMN_SCORE) {
        int i, x, y, w = sw >= 300 ? 230 : 196, h = sh >= 200 ? 170 : 128, a = H->sc_age, ry;
        x = (sw - w) / 2;
        y = (sh - h) / 2;
        if (!ui_window_pop(x, y, w, h, WIN_CUTE, (int)H->t)) return;
        rescore();
        ui_text(x + 8, y + 6, home_room_defs[H->room].name, st->text);
        for (i = 0; i < HS_COUNT; i++) {
            /* the counter: v = N * t^2 (0.8 + 0.2 t) over 22 frames, a tick every 2 */
            fix16 t = tw_t(a, ht[HT_COUNTER]);
            int64_t tt = (int64_t)t * t / FIX_ONE;
            int v = (int)((int64_t)H->score.stat[i] * tt / FIX_ONE * (FIX_ONE * 4 / 5 + t / 5) / FIX_ONE);
            ry = y + 18 + i * (sh >= 200 ? 13 : 11);
            ui_text(x + 8, ry, stat_names[i], st->text);
            ui_bar(x + 72, ry + 1, w - 110, 6, v, 100, stat_cols[i]);
            doomo_snprintf(buf, sizeof(buf), "%d", v);
            ui_text(x + w - 32, ry, buf, st->text_dim);
        }
        if (a < ht[HT_COUNTER] && a % ht[HT_COUNTER_TICK] == 0) audio_sfx_synth(SFX_TIK);
        /* the feng-shui rules that fired */
        ry = y + 20 + HS_COUNT * (sh >= 200 ? 13 : 11);
        for (i = 0; i < 13 && ry < y + h - 24; i++)
            if (H->score.rules & (1u << i)) {
                ui_text(x + 10, ry, rule_text[i], (i == 2 || i == 6 || i == 9 || i >= 11) ? PAL_UI_RED : PAL_UI_MINT_D);
                ry += 10;
            }
        if (a >= ht[HT_COUNTER]) {
            int g = home_stamps_grade(H->score.stamps), s = tw_tab(&TW_SLAM_STAMP, a - ht[HT_COUNTER]);
            const char *gn = grade_names[g];
            int scale = s > 400 ? 3 : 2, tw = ui_text_big_width(gn, scale);
            if (a == ht[HT_COUNTER]) {
                camfx_shake(SHK_STAMP);
                if (g > H->sc_last) audio_sfx_synth(SFX_FANFARE);
                else if (g < H->sc_last) audio_sfx_synth(SFX_KAZOO);
                H->sc_last = (uint8_t)g;
            }
            ui_text_big(x + w - tw - 10, y + h - 22, gn, scale, PAL_UI_RED, PAL_WHITE);
        }
        doomo_snprintf(buf, sizeof(buf), "Pieces %d  Lamps %d  Seats %d", H->score.pieces, H->score.lamps, H->score.seats);
        ui_text(x + 8, y + h - 12, buf, st->text_dim);
        return;
    }
    if (H->menu == HMN_WALLS) {
        static const char *const rown[5] = { "Wallpaper", "Wall paint", "Floor", "Floor paint", "Paint it!" };
        static const char *const wpn[8] = { "Stripes", "Dots", "Wainscot", "Panels", "Hearts", "Harlequin", "Stone", "Stars" };
        static const char *const fln[8] = { "Checker", "Planks", "Tiles", "Herringbone", "Grass", "Flagstone", "Lava crack", "Cloud" };
        int w = sw >= 300 ? 200 : 170, h = 5 * 14 + 20, x = (sw - w) / 2, y = 20, i;
        if (!ui_window_pop(x, y, w, h, WIN_CUTE, (int)H->t)) return;
        for (i = 0; i < 5; i++) {
            const char *v = i == 0 ? wpn[H->wall_v[0] & 7] : i == 2 ? fln[H->wall_v[2] & 7] :
                            i == 1 ? doomo_paint_names[H->wall_v[1] & 31] : i == 3 ? doomo_paint_names[H->wall_v[3] & 31] : "";
            ui_value_row(x + 6, y + 8 + i * 14, (i == 1 || i == 3) ? w - 26 : w - 12, rown[i], v, i == H->wall_row);
            if (i == 1 || i == 3) ui_round_rect(x + w - 17, y + 9 + i * 14, 10, 9, 2, PAL_PAINT(H->wall_v[i] & 31, 1));
        }
        return;
    }
    {
        /* the list modals: catalog, themes, blueprints, rooms */
        int lw = sw >= 300 ? 150 : 120, x = 6, y = 16, h = sh - 34, top = y + 16;
        const char *title = H->menu == HMN_CATALOG ? (H->gift_mode ? "Give a gift" : "Catalog") :
                            H->menu == HMN_THEMES ? "Themes" : H->menu == HMN_BLUEPRINTS ? "Blueprints" : "Rooms";
        int px = x + lw + 10, pw = sw - 12 - lw - 14;
        if (!ui_window_pop(x, y, sw - 12, h, WIN_CUTE, (int)H->t)) return;
        ui_text(x + 6, y + 4, title, st->text);
        if (H->menu == HMN_CATALOG && H->ncat_tabs) {
            doomo_snprintf(buf, sizeof(buf), "< %s >", cat_names[H->cat_tabs[H->cat_tab]]);
            ui_text(x + 6 + ui_text_width(title) + 8, y + 4, buf, st->accent);
        }
        draw_list_rows(x + 6, top, lw, rows, H->menu == HMN_CATALOG ? label_catalog : H->menu == HMN_THEMES ? label_theme :
                       H->menu == HMN_BLUEPRINTS ? label_bp : label_room, st);
        if (H->menu == HMN_CATALOG && H->nrows > 0) {
            /* the preview over the description, the price line under both */
            int furn = H->rows[H->sel], lines, dy;
            const DoomoFurn *f = home_furn(furn);
            const char *desc;
            if (!f) return;
            desc = doomo_furn_text(f->desc, buf, sizeof(buf));
            lines = ui_text_wrap(px, 0, pw, desc, 0, 0);
            dy = y + h - 15 - lines * FONT_LINE;
            plat_rect(px, top, pw, dy - 3 - top, st->fill_lo);
            draw_sprite_preview(furn, px, top, pw, dy - 3 - top, cache, cap);
            ui_text_wrap(px, dy, pw, desc, st->text, 1);
            doomo_snprintf(buf, sizeof(buf), f->price ? "H$%u" : "gift", (unsigned)f->price);
            ui_text(px, y + h - 12, buf, PAL_UI_GOLD);
            doomo_snprintf(buf, sizeof(buf), "x%d", avail(furn));
            ui_text(px + pw - ui_text_width(buf) - 2, y + h - 12, buf, st->text_dim);
            return;
        }
        plat_rect(px, top, pw, h - 52, st->fill_lo);
        if (H->nrows <= 0) return;
        if (H->menu == HMN_THEMES) {
            const DoomoHomeTheme *t = &doomo_home_themes[H->sel];
            int k, sx = px + 6;
            const uint8_t pp[4] = { t->wall_paint, t->floor_paint, t->paint_a, t->paint_b };
            for (k = 0; k < 4; k++) { ui_round_rect(sx + k * 18, top + 10, 14, 14, 3, PAL_PAINT(pp[k] & 31, 1)); }
            ui_text_wrap(px + 4, top + 32, pw - 8, "Walls, floor and every piece you haven't painted.", st->text, 1);
        } else if (H->menu == HMN_BLUEPRINTS) {
            int r = H->rows[H->sel];
            if (r & 0x8000) ui_text_wrap(px + 4, top + 8, pw - 8, "Keep this room as one of your two blueprints.", st->text, 1);
            else if (r < (int)doomo_home_blueprint_count) {
                const DoomoHomeSet *b = &doomo_home_blueprints[r];
                doomo_snprintf(buf, sizeof(buf), "%s, %d pieces", home_room_defs[b->room < HOME_ROOM_COUNT ? b->room : 0].name, b->n);
                ui_text_wrap(px + 4, top + 8, pw - 8, buf, st->text, 1);
                ui_text_wrap(px + 4, top + 30, pw - 8, "What you own is placed; the rest are grey ghosts.", st->text_dim, 1);
            }
        } else {
            const HomeRoomDef *d = &home_room_defs[H->rows[H->sel]];
            doomo_snprintf(buf, sizeof(buf), "%dx%d  %s", d->w, d->d, grade_names[home_grade(H->rows[H->sel])]);
            ui_text(px + 4, top + 8, buf, st->text);
        }
    }
}

/* the lane strip of Hold the Door: each lane's demons sliding toward the
 * wall (d->lanes is a bit per lane: the door, then the window slots) */
static void draw_defend(void)
{
    const HomeDefend *d = &H->def;
    int sw = plat_screen_w(), bw = sw >= 300 ? 116 : 100, x0 = sw - bw - 3, y0 = 16, len = bw - 34;
    int lane, i, n = 0, ly;
    static const char *const lane_names[HDL_COUNT] = { "Door", "Win 1", "Win 2" };
    char buf[40];
    if (H->def_card) {
        /* dawn: the result card alone */
        int w = 170, h = 50, x = (sw - w) / 2, y = 40, safe = home_defend_safe(d);
        if (H->hn_age < 0xFF) H->hn_age++;             /* the pop runs once */
        if (ui_window_pop(x, y, w, h, WIN_CUTE, (int)H->hn_age)) {
            ui_text_center(sw / 2, y + 8, "Dawn.", ui_style(WIN_CUTE)->text);
            doomo_snprintf(buf, sizeof(buf), "%d of %d kept safe%s", safe, HDG_COUNT, safe >= 4 ? "  HELD" : "");
            ui_text_center(sw / 2, y + 22, buf, safe >= 4 ? PAL_UI_RED : ui_style(WIN_CUTE)->text_dim);
            ui_text_center(sw / 2, y + 36, STR_ICON_A " go outside", ui_style(WIN_CUTE)->text_dim);
        }
        return;
    }
    for (lane = 0; lane < HDL_COUNT; lane++) n += (d->lanes >> lane) & 1;
    ui_round_rect(x0 - 4, y0 - 3, bw + 6, n * 10 + 25, 3, PAL_BLACK);
    ly = y0;
    for (lane = 0; lane < HDL_COUNT; lane++) {
        if (!((d->lanes >> lane) & 1)) continue;
        ui_text(x0, ly, lane_names[lane], PAL_WHITE);
        plat_rect(x0 + 32, ly + 4, len, 1, PAL_GRAY_D);
        plat_rect(x0 + 29, ly + 1, 2, 7, PAL_RAMP(RAMP_WOOD, 3));         /* the wall */
        for (i = 0; i < HD_DEMONS; i++) {
            const HomeDemon *m = &d->dm[i];
            int px;
            if (m->lane != lane || (m->state != HDS_WALK && m->state != HDS_HIT && m->state != HDS_STARE && m->state != HDS_FLEE)) continue;
            px = x0 + 32 + DOOMO_CLAMP(m->dist, 0, 54) * len / 54;
            ui_fill_circle(px, ly + 4, m->kind == HDK_BRUTE ? 3 : 2, m->state == HDS_FLEE ? PAL_UI_GOLD : PAL_UI_RED);
        }
        ly += 10;
    }
    {
        int mx = d->door_max > 0 ? d->door_max : 100;
        ly += 2;
        ui_text(x0, ly, "HP", PAL_WHITE);
        ui_bar(x0 + 32, ly + 1, len, 6, d->door_broken ? 0 : d->door_hp, mx, d->door_broken ? PAL_UI_RED : PAL_UI_MINT);
        doomo_snprintf(buf, sizeof(buf), "Wave %d/%d", d->wave + 1, d->nwaves);
        ui_text(x0, ly + 10, buf, PAL_UI_GOLD);
        if (d->turret >= 0) {                           /* the toaster turret's rounds */
            doomo_snprintf(buf, sizeof(buf), "Ammo %d", d->ammo);
            ui_text(x0 + bw - ui_text_width(buf), ly + 10, buf, d->ammo ? PAL_UI_GOLD : PAL_UI_RED);
        }
    }
    if (d->busy) {
        int cx, cy;
        Proj p;
        proj_setup(&p);
        tile_center(&p, H->cx, H->cy, 18, &cx, &cy);
        ui_bar(cx - 8, cy, 16, 3, d->busy, 12, PAL_UI_GOLD);
    }
}

/* the reaction over an actor: !, hearts, the bubble, the sweat drop */
static void draw_reaction(const DoomoRoomView *v)
{
    const DoomoRoomActor *a = NULL;
    int sx, sy, age = H->react_age, idx;
    Proj p;
    if (age == 0xFF || H->react == HVR_NONE) return;
    idx = H->react_actor == AC_HOST ? (H->nact > 1 ? 1 : 0) : 0;
    if (H->react_actor >= AC_VISITOR0 && H->react_actor < AC_VISITOR0 + 3) {
        int k = H->react_actor - AC_VISITOR0, i, seen = 0;
        for (i = 0; i < H->nact; i++)
            if (H->act[i].img != &spr_pet && i > 0 && seen++ == k) { idx = i; break; }
    }
    if (idx >= v->nact) return;
    a = &v->actors[idx];
    proj_setup(&p);
    proj(&p, a->x, a->y, a->z + 28, &sx, &sy);
    if (H->react == HVR_DOUBLE_TAKE && age < ht[HT_REACT_LOOK] + ht[HT_REACT_SHOCK] + ht[HT_REACT_STARE]) {
        if (age >= ht[HT_REACT_LOOK] && age < ht[HT_REACT_LOOK] + ht[HT_REACT_SHOCK])
            ui_text_big(sx - 3, sy - 8, "!", 2, PAL_UI_RED, PAL_WHITE);
        return;
    }
    if (H->react == HVR_DAWNING && age < ht[HT_REACT_LOOK] + 12) {
        int b = tw_tab(&TW_POP_EYE, age);
        ui_text_big(sx - 3, sy - 6 - (b - 256) / 32, "!", 2, PAL_UI_GOLD, PAL_WHITE);
        return;
    }
    if (H->react == HVR_HEARTS) {
        int s = tw_tab(&TW_POP_BURST, age), k;
        for (k = 0; k < 3; k++) ui_heart(sx - 8 + k * 8, sy - 4 - (k & 1) * 4 - ui_wave(H->t + k * 9, ht[HT_BREATH], 1),
                                         3 + (s - 256) / 128, PAL_UI_PINK);
    } else if (H->react == HVR_BANG) {
        ui_text_big(sx - 3, sy - 8, "!", 2, PAL_UI_RED, PAL_WHITE);
    } else if (H->react == HVR_EW) {
        plat_rect(sx + 6, sy + 6 + (age >> 1) % 8, 2, 3, PAL_RAMP(RAMP_SKY, 6));   /* the sweat drop */
    }
    if (H->react_line && H->react_line[0] && H->react != HVR_FLEE) {
        const UiStyle *st = ui_style(WIN_CUTE);
        int w = ui_text_width(H->react_line) + 8, bx, by;
        int dy = H->react == HVR_DEADPAN ? (age < 5 ? (5 - age) * 2 : 0) : (tw_tab(&TW_POP_BUBBLE, age) - 256) / 64;
        if (H->kind == HK_VISIT && H->visit.kind == HVK_SEALED) return;   /* no bubbles in the sealed room */
        bx = DOOMO_CLAMP(sx - w / 2, 2, plat_screen_w() - w - 2);
        by = DOOMO_CLAMP(sy - 22 + dy, 14, plat_screen_h() - 40);
        ui_round_rect(bx, by, w, 12, 4, H->react == HVR_DEADPAN ? st->fill_lo : st->fill_hi);
        ui_text(bx + 4, by + 2, H->react_line, st->text);
    }
}

static void draw_effects(void)
{
    int i, sx, sy;
    Proj p;
    proj_setup(&p);
    /* the DROP's corner dust and the paint splats */
    for (i = 0; i < H->ed.room.nobj && i < HOME_OBJ_MAX; i++) {
        int k = H->oa_k[i], a = H->oa_t[i];
        const HomeObj *o = &H->ed.room.obj[i];
        if (k == OA_DROP && a >= ht[HT_DROP_FALL] && a < ht[HT_DROP_FALL] + ht[HT_DUST] + 1) {
            const DoomoFurn *f = home_furn(HO_FURN(o));
            int w = 1, d = 1, c;
            if (f) home_footprint(f, HO_ROT(o), &w, &d);
            for (c = 0; c < 4; c++) {
                proj(&p, INT2FIX(HO_X(o) + ((c & 1) ? w : 0)), INT2FIX(HO_Y(o) + ((c & 2) ? d : 0)), 0, &sx, &sy);
                ui_fill_circle(sx + ((c & 1) ? 2 : -2), sy - 1 - (a - ht[HT_DROP_FALL]), 2 + (a - ht[HT_DROP_FALL]),
                               PAL_RAMP(RAMP_BONE, 7));
            }
        } else if (k == OA_SPLAT) {
            int s = tw_tab(&TW_POP_BUBBLE, a), pa = o->pa & 0x1F;
            tile_center(&p, HO_X(o), HO_Y(o), o->z * 4 + 8, &sx, &sy);
            ui_fill_circle(sx, sy, 6 * s / 256 + 2, PAL_PAINT(pa == FURN_PAINT_DEFAULT ? PAINT_BUBBLEGUM : pa, 1));
            ui_fill_circle(sx - 5, sy + 3, 2 * s / 256 + 1, PAL_PAINT(pa == FURN_PAINT_DEFAULT ? PAINT_BUBBLEGUM : pa, 2));
        }
    }
    /* the expansion: the dust, then the new floor tiles arriving diagonal by diagonal */
    if (H->exp_age != 0xFF) {
        int a = H->exp_age, diag = (a - ht[HT_RUMBLE] - ht[HT_RUMBLE_QUIET]) / ht[HT_TILE_GAP], x, y;
        if (a < ht[HT_RUMBLE] && !(a & 1))
            for (i = 0; i < 3; i++) {
                tile_center(&p, (a + i * 3) % room_w(), 0, 40 - a * 2, &sx, &sy);
                ui_fill_circle(sx, sy, 3, PAL_RAMP(RAMP_BONE, 6));
            }
        for (y = 0; y < room_d(); y++)
            for (x = 0; x < room_w(); x++) {
                int k = (room_w() - 1 - x) + (room_d() - 1 - y);   /* from the far corner */
                if (a < ht[HT_RUMBLE] + ht[HT_RUMBLE_QUIET] || k > diag) {
                    tile_center(&p, x, y, 0, &sx, &sy);
                    plat_rect(sx - 10, sy - 3, 20, 7, PAL_BLACK);
                }
            }
    }
    /* photo stickers */
    if (H->mode == HM_PHOTO || H->mode == HM_VIEW)
        for (i = 0; i < 3; i++)
            if (H->photo.sticker[i] && H->photo.sticker[i] < 4) {
                int x = 8 + H->photo.sx[i] * (plat_screen_w() - 40) / 15, y = 20 + H->photo.sy[i] * (plat_screen_h() - 60) / 7;
                if (H->photo.sticker[i] == 2) {
                    ui_round_rect(x - 2, y - 2, ui_text_width(sticker_names[2]) + 4, 12, 3, PAL_WHITE);
                    ui_text(x, y, sticker_names[2], PAL_UI_PINK_D);
                } else if (H->photo.sticker[i] == 1) {
                    ui_heart(x + 4, y + 4, 5, PAL_UI_PINK);
                } else {
                    ui_text_big(x, y, sticker_names[3], 2, PAL_RAMP(RAMP_WOOD, 2), PAL_WHITE);
                }
            }
}

static void draw_story_panel(void)
{
    const UiStyle *st = ui_style(WIN_CUTE);
    char buf[48];
    int y = 16;
    if (H->kind != HK_STORY) return;
    if (H->story == HSM_TUTORIAL) {
        static const char *const items[3] = { "the Picnic Crate", "the Egg Nest", "a lamp" };
        int k, ok[3];
        ok[0] = tutorial_has(-1, FURN_PICNIC_CRATE); ok[1] = tutorial_has(FR_NEST, -1); ok[2] = tutorial_has(FR_LAMP, -1);
        ui_round_rect(2, y, 112, 36, 3, st->fill);
        for (k = 0; k < 3; k++) {
            doomo_snprintf(buf, sizeof(buf), "%s %s", ok[k] ? STR_ICON_HEART : "-", items[k]);
            ui_text(6, y + 2 + k * 11, buf, ok[k] ? PAL_UI_MINT_D : st->text);
        }
    } else if (H->story == HSM_GOAL && H->param < HG_COUNT) {
        doomo_snprintf(buf, sizeof(buf), "Goal: %s", grade_names[H->param]);
        ui_round_rect(2, y, ui_text_width(buf) + 8, 12, 3, st->fill);
        ui_text(6, y + 2, buf, st->text);
    } else if (H->story == HSM_DECORATE_FOR) {
        const DoomoHomeSet *l = home_layout_find(H->param);
        int prog = home_decorate_for(H->param, -1);
        doomo_snprintf(buf, sizeof(buf), "%s  %d/8", l ? l->name : "Decorate", prog);
        ui_round_rect(2, y, ui_text_width(buf) + 8, 20, 3, st->fill);
        ui_text(6, y + 2, buf, st->text);
        ui_bar(6, y + 12, ui_text_width(buf), 4, prog, 8, PAL_UI_PINK);
    }
}

static void draw_stamp(void)
{
    int a = H->sc_age, sw = plat_screen_w(), sh = plat_screen_h();
    const char *label;
    int s, scale, tw;
    if (a == 0xFF || H->menu == HMN_SCORE) return;
    label = H->sc_grade == 0xFE ? "BLUEPRINT" : H->sc_grade == 0xFD ? "DONE!" :
            H->sc_grade < HG_COUNT ? grade_names[H->sc_grade] : "";
    if (!label[0] || (H->sc_grade == HG_NONE)) return;
    s = tw_tab(&TW_SLAM_STAMP, a);
    scale = s > 500 ? 4 : s > 300 ? 3 : 2;
    tw = ui_text_big_width(label, scale);
    ui_text_big(sw / 2 - tw / 2, sh / 2 - 20, label, scale, PAL_UI_RED, PAL_WHITE);
}

static void home_draw(void)
{
    DoomoRoomView v;
    uint8_t *base, *cache;
    uint32_t used, cap;
    int sw, sh;
    reclaim();                                  /* the frame a pushed scene pops: it had the scratch */
    if (!H || !scene_scratch_mine(HD_TAG)) { plat_clear(PAL_BLACK); return; }
    plat_code_part(H->part);
    sw = plat_screen_w();
    sh = plat_screen_h();
    base = (uint8_t *)H;
    used = ((uint32_t)sizeof(HomeScn) + 255u) & ~255u;
    cache = base + used;
    cap = DOOMO_SCENE_SCRATCH > used ? DOOMO_SCENE_SCRATCH - used : 0;
    build_view(&v, cache, cap);
    build_actors(&v);
    plat_room(&v);
    if (plat_draw_skipped()) { camfx_apply(); return; }
    draw_effects();
    draw_reaction(&v);
    camfx_apply();
    if (H->mode == HM_PHOTO || H->mode == HM_VIEW) {
        /* the camera's letterbox where the HUD bars were */
        plat_rect(0, 0, sw, 12, PAL_BLACK);
        plat_rect(0, sh - 12, sw, 12, PAL_BLACK);
        plat_room_ui_opaque(0, 0, sw, 12);
        plat_room_ui_opaque(0, sh - 12, sw, 12);
        if (H->mode == HM_PHOTO && (H->shot_age == 0xFF || H->shot_age > ht[HT_SHUTTER])) {
            ui_text(4, 4, "PHOTO", PAL_WHITE);
            const char *ph = "A:snap  B:sticker  SEL:filter  L/R:turn  START:done";
            ui_text(4, sh - 10, ui_text_width(ph) <= sw - 8 ? ph : "A:snap B:sticker SEL:filter START:done", PAL_WHITE);
        }
        if (H->mode == HM_VIEW) {
            char buf[48];
            int changed = 0;
            if (H->photo.place >= HOME_PHOTO_OWN) changed = home_room_hash(&H->ed.room) != H->photo.hash;
            doomo_snprintf(buf, sizeof(buf), "Photo %d%s", H->view_slot + 1, changed ? "  (the room has changed since)" : "");
            ui_round_rect(2, sh - 14, ui_text_width(buf) + 8, 12, 3, PAL_BLACK);
            ui_text(6, sh - 12, buf, PAL_WHITE);
        }
        if (H->shot_age != 0xFF && H->shot_age < ht[HT_SHUTTER]) plat_post_blend(PAL_WHITE, 7);
        draw_caption();
        return;
    }
    draw_hud();
    draw_story_panel();
    if (H->mode == HM_DEFEND) draw_defend();
    if (H->menu != HMN_NONE) draw_menu(cache, cap);
    draw_stamp();
    draw_caption();
    if (H->sleep_age != 0xFF && H->sleep_age >= ht[HT_SLEEP_FADE] && H->sleep_age < ht[HT_SLEEP_FADE] + ht[HT_SLEEP_MOON])
        plat_post_blend(PAL_RAMP(RAMP_SKY, 1), 8);           /* the room by moonlight */
    if (H->t < ht[HT_IRIS]) {
        /* the heart iris in: r = 6 + 250 * smooth(a / 6)^2 */
        fix16 s = tw_smooth(tw_t((int)H->t, ht[HT_IRIS]));
        int r = 6 + (int)(((int64_t)250 * s / FIX_ONE) * s / FIX_ONE);
        plat_post_iris(sw / 2, sh / 2, r, 1, PAL_UI_PINK);
    }
}

const DoomoScene SCENE_HOME = { "HOME", home_enter, home_leave, home_update, home_draw, 0 };
